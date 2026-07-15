#include "block.h"
#include "kprintf.h"
#include "virtio_blk.h"

struct block_stats
{
    unsigned long reads;
    unsigned long writes;
    unsigned long read_sectors;
    unsigned long written_sectors;
    unsigned long rejected;
    int self_test_passed;
};

static unsigned char ram_disk[BLOCK_RAM_SECTORS][BLOCK_SECTOR_SIZE]
    __attribute__((aligned(BLOCK_SECTOR_SIZE)));
static struct block_stats stats;
static int use_virtio;

void block_init(void)
{
    stats.reads = 0;
    stats.writes = 0;
    stats.read_sectors = 0;
    stats.written_sectors = 0;
    stats.rejected = 0;
    stats.self_test_passed = 0;
    use_virtio = virtio_blk_init();

    if (!use_virtio)
    {
        for (unsigned long sector = 0; sector < BLOCK_RAM_SECTORS; sector++)
        {
            for (unsigned long i = 0; i < BLOCK_SECTOR_SIZE; i++)
            {
                ram_disk[sector][i] = 0;
            }
        }
    }
}

int block_read(unsigned long sector, void *buffer, unsigned long count)
{
    unsigned long sectors = block_sector_count();

    if (!buffer || !count || sector >= sectors || count > sectors - sector)
    {
        stats.rejected++;
        return 0;
    }
    if (use_virtio)
    {
        if (!virtio_blk_read(sector, buffer, count))
        {
            stats.rejected++;
            return 0;
        }
    }
    else for (unsigned long current = 0; current < count; current++)
    {
        for (unsigned long i = 0; i < BLOCK_SECTOR_SIZE; i++)
        {
            ((unsigned char *)buffer)[current * BLOCK_SECTOR_SIZE + i] =
                ram_disk[sector + current][i];
        }
    }
    stats.reads++;
    stats.read_sectors += count;
    return 1;
}

int block_write(unsigned long sector, const void *buffer, unsigned long count)
{
    unsigned long sectors = block_sector_count();

    if (!buffer || !count || sector >= sectors || count > sectors - sector)
    {
        stats.rejected++;
        return 0;
    }
    if (use_virtio)
    {
        if (!virtio_blk_write(sector, buffer, count))
        {
            stats.rejected++;
            return 0;
        }
    }
    else for (unsigned long current = 0; current < count; current++)
    {
        for (unsigned long i = 0; i < BLOCK_SECTOR_SIZE; i++)
        {
            ram_disk[sector + current][i] =
                ((const unsigned char *)buffer)
                    [current * BLOCK_SECTOR_SIZE + i];
        }
    }
    stats.writes++;
    stats.written_sectors += count;
    return 1;
}

int block_self_test(void)
{
    unsigned char saved[BLOCK_SECTOR_SIZE];
    unsigned char pattern[BLOCK_SECTOR_SIZE];
    unsigned char result[BLOCK_SECTOR_SIZE];
    unsigned long sector = block_sector_count() - 1UL;
    int passed = 1;

    if (!block_read(sector, saved, 1))
    {
        return 0;
    }
    for (unsigned long i = 0; i < BLOCK_SECTOR_SIZE; i++)
    {
        pattern[i] = (unsigned char)(i ^ 0xA5U);
    }
    if (!block_write(sector, pattern, 1) ||
        !block_read(sector, result, 1))
    {
        passed = 0;
    }
    for (unsigned long i = 0; i < BLOCK_SECTOR_SIZE; i++)
    {
        if (result[i] != pattern[i])
        {
            passed = 0;
            break;
        }
    }
    if (!block_write(sector, saved, 1))
    {
        passed = 0;
    }
    stats.self_test_passed = passed;
    return passed;
}

void block_dump_stats(void)
{
    kprintf("Block device: %s sectors=%d sector_size=%d capacity=%d KB\n",
            use_virtio ? "virtio-blk" : "ramdisk",
            (int)block_sector_count(),
            (int)BLOCK_SECTOR_SIZE,
            (int)((block_sector_count() * BLOCK_SECTOR_SIZE) / 1024UL));
    kprintf("  reads=%d sectors=%d writes=%d sectors=%d rejected=%d\n",
            (int)stats.reads,
            (int)stats.read_sectors,
            (int)stats.writes,
            (int)stats.written_sectors,
            (int)stats.rejected);
    kprintf("  self_test=%s\n", stats.self_test_passed ? "passed" : "pending");
}

unsigned long block_sector_count(void)
{
    return use_virtio ? virtio_blk_sector_count() : BLOCK_RAM_SECTORS;
}

const char *block_device_name(void)
{
    return use_virtio ? "virtio-blk" : "ramdisk";
}
