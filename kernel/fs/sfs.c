#include "sfs.h"
#include "block.h"
#include "kprintf.h"
#include <stdint.h>

#define SFS_MAGIC 0x53465331U
#define SFS_VERSION 1U
#define SFS_SUPER_SECTOR 0UL
#define SFS_DIRECTORY_SECTOR 1UL
#define SFS_DATA_START 2UL
#define SFS_ENTRY_USED 1U

struct sfs_superblock
{
    uint32_t magic;
    uint32_t version;
    uint32_t sector_size;
    uint32_t total_sectors;
    uint32_t max_files;
    uint32_t data_start;
    uint8_t reserved[488];
};

struct sfs_entry
{
    char path[SFS_MAX_PATH];
    uint32_t start_sector;
    uint32_t size;
    uint32_t checksum;
    uint32_t flags;
};

struct sfs_directory
{
    struct sfs_entry entries[SFS_MAX_FILES];
    uint8_t reserved[32];
};

static struct sfs_superblock superblock;
static struct sfs_directory directory;
static unsigned char io_buffer[SFS_MAX_FILE_SIZE]
    __attribute__((aligned(512)));
static int mounted;

static int path_equals(const char *left, const char *right)
{
    while (*left && *right && *left == *right)
    {
        left++;
        right++;
    }
    return *left == *right;
}

static int path_valid(const char *path)
{
    unsigned long length = 0;

    if (!path || path[0] != '/')
    {
        return 0;
    }
    while (path[length])
    {
        length++;
        if (length >= SFS_MAX_PATH)
        {
            return 0;
        }
    }
    return length > 1;
}

static uint32_t checksum(const unsigned char *data, unsigned long size)
{
    uint32_t value = 0;
    for (unsigned long i = 0; i < size; i++)
    {
        value = (value << 5) ^ (value >> 2) ^ data[i];
    }
    return value;
}

static struct sfs_entry *find_entry(const char *path)
{
    for (unsigned long i = 0; i < SFS_MAX_FILES; i++)
    {
        if ((directory.entries[i].flags & SFS_ENTRY_USED) &&
            path_equals(directory.entries[i].path, path))
        {
            return &directory.entries[i];
        }
    }
    return 0;
}

static int write_directory(void)
{
    return block_write(SFS_DIRECTORY_SECTOR, &directory, 1);
}

int sfs_format(void)
{
    unsigned long required = SFS_DATA_START +
                             SFS_MAX_FILES * SFS_FILE_SECTORS;

    if (block_sector_count() < required)
    {
        mounted = 0;
        return 0;
    }
    superblock.magic = SFS_MAGIC;
    superblock.version = SFS_VERSION;
    superblock.sector_size = 512U;
    superblock.total_sectors = (uint32_t)block_sector_count();
    superblock.max_files = SFS_MAX_FILES;
    superblock.data_start = SFS_DATA_START;
    for (unsigned long i = 0; i < sizeof(superblock.reserved); i++)
    {
        superblock.reserved[i] = 0;
    }
    for (unsigned long i = 0; i < SFS_MAX_FILES; i++)
    {
        directory.entries[i].path[0] = '\0';
        directory.entries[i].start_sector =
            (uint32_t)(SFS_DATA_START + i * SFS_FILE_SECTORS);
        directory.entries[i].size = 0;
        directory.entries[i].checksum = 0;
        directory.entries[i].flags = 0;
    }
    for (unsigned long i = 0; i < sizeof(directory.reserved); i++)
    {
        directory.reserved[i] = 0;
    }
    mounted = block_write(SFS_SUPER_SECTOR, &superblock, 1) &&
              write_directory();
    return mounted;
}

void sfs_init(void)
{
    struct sfs_superblock disk_super;

    mounted = 0;
    if (!block_read(SFS_SUPER_SECTOR, &disk_super, 1))
    {
        return;
    }
    if (disk_super.magic == 0 && disk_super.version == 0)
    {
        sfs_format();
        return;
    }
    if (disk_super.magic != SFS_MAGIC ||
        disk_super.version != SFS_VERSION ||
        disk_super.sector_size != 512U ||
        disk_super.max_files != SFS_MAX_FILES ||
        disk_super.data_start != SFS_DATA_START ||
        disk_super.total_sectors > block_sector_count())
    {
        return;
    }
    if (!block_read(SFS_DIRECTORY_SECTOR, &directory, 1))
    {
        return;
    }
    superblock.magic = disk_super.magic;
    superblock.version = disk_super.version;
    superblock.sector_size = disk_super.sector_size;
    superblock.total_sectors = disk_super.total_sectors;
    superblock.max_files = disk_super.max_files;
    superblock.data_start = disk_super.data_start;
    for (unsigned long i = 0; i < sizeof(superblock.reserved); i++)
    {
        superblock.reserved[i] = disk_super.reserved[i];
    }
    mounted = 1;
}

int sfs_write_file(const char *path, const void *data, unsigned long size)
{
    struct sfs_entry *entry;

    if (!mounted || !path_valid(path) || (size && !data) ||
        size > SFS_MAX_FILE_SIZE)
    {
        return 0;
    }
    entry = find_entry(path);
    if (!entry)
    {
        for (unsigned long i = 0; i < SFS_MAX_FILES; i++)
        {
            if (!(directory.entries[i].flags & SFS_ENTRY_USED))
            {
                entry = &directory.entries[i];
                break;
            }
        }
    }
    if (!entry)
    {
        return 0;
    }
    for (unsigned long i = 0; i < SFS_MAX_FILE_SIZE; i++)
    {
        io_buffer[i] = i < size ? ((const unsigned char *)data)[i] : 0;
    }
    if (!block_write(entry->start_sector, io_buffer, SFS_FILE_SECTORS))
    {
        return 0;
    }
    unsigned long i = 0;
    for (; path[i]; i++)
    {
        entry->path[i] = path[i];
    }
    entry->path[i] = '\0';
    entry->size = (uint32_t)size;
    entry->checksum = checksum((const unsigned char *)data, size);
    entry->flags = SFS_ENTRY_USED;
    return write_directory();
}

long sfs_read_file(const char *path, void *data, unsigned long capacity)
{
    struct sfs_entry *entry;

    if (!mounted || !data)
    {
        return -1;
    }
    entry = find_entry(path);
    if (!entry || entry->size > capacity || entry->size > SFS_MAX_FILE_SIZE ||
        !block_read(entry->start_sector, io_buffer, SFS_FILE_SECTORS) ||
        checksum(io_buffer, entry->size) != entry->checksum)
    {
        return -1;
    }
    for (unsigned long i = 0; i < entry->size; i++)
    {
        ((unsigned char *)data)[i] = io_buffer[i];
    }
    return (long)entry->size;
}

void sfs_dump(void)
{
    kprintf("SaturnFS: state=%s files=%d max_file=%d\n",
            mounted ? "mounted" : "unavailable",
            (int)SFS_MAX_FILES,
            (int)SFS_MAX_FILE_SIZE);
    if (!mounted)
    {
        return;
    }
    for (unsigned long i = 0; i < SFS_MAX_FILES; i++)
    {
        if (directory.entries[i].flags & SFS_ENTRY_USED)
        {
            kprintf("  %s size=%d sector=%d checksum=0x%x\n",
                    directory.entries[i].path,
                    (int)directory.entries[i].size,
                    (int)directory.entries[i].start_sector,
                    directory.entries[i].checksum);
        }
    }
}

int sfs_ready(void)
{
    return mounted;
}
