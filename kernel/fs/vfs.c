#include "vfs.h"
#include "kprintf.h"
#include "sfs.h"

enum ramfs_node_kind
{
    RAMFS_FILE = 1,
    RAMFS_DIRECTORY,
};

struct ramfs_node
{
    char path[VFS_MAX_PATH];
    unsigned long size;
    unsigned char data[VFS_MAX_FILE_SIZE];
    enum ramfs_node_kind kind;
    int used;
};

static struct ramfs_node nodes[VFS_MAX_NODES];
static unsigned long file_count;
static unsigned long dir_count;
static unsigned char disk_file_buffer[SFS_MAX_FILE_SIZE];

static const char *vfs_disk_path(const char *path)
{
    static const char prefix[] = "/disk";
    unsigned long i = 0;

    if (!path)
    {
        return 0;
    }
    while (prefix[i])
    {
        if (path[i] != prefix[i])
        {
            return 0;
        }
        i++;
    }
    return path[i] == '/' && path[i + 1] ? path + i : 0;
}

static int vfs_path_valid(const char *path)
{
    unsigned long length = 0;

    if (!path || path[0] != '/')
    {
        return 0;
    }

    while (path[length])
    {
        length++;
        if (length >= VFS_MAX_PATH)
        {
            return 0;
        }
    }

    return length > 1;
}

static int vfs_path_equals(const char *left, const char *right)
{
    while (*left && *right && *left == *right)
    {
        left++;
        right++;
    }

    return *left == *right;
}

static struct ramfs_node *vfs_find(const char *path)
{
    for (unsigned long i = 0; i < VFS_MAX_NODES; i++)
    {
        if (nodes[i].used && vfs_path_equals(nodes[i].path, path))
        {
            return &nodes[i];
        }
    }

    return 0;
}

static int vfs_parent_exists(const char *path)
{
    char parent[VFS_MAX_PATH];
    unsigned long last_slash = 0;
    unsigned long length = 0;
    struct ramfs_node *node;

    while (path[length])
    {
        if (path[length] == '/')
        {
            last_slash = length;
        }
        length++;
    }
    if (last_slash == 0)
    {
        return 1;
    }
    for (unsigned long i = 0; i < last_slash; i++)
    {
        parent[i] = path[i];
    }
    parent[last_slash] = '\0';
    node = vfs_find(parent);
    return node && node->kind == RAMFS_DIRECTORY;
}

static struct ramfs_node *vfs_alloc_node(void)
{
    for (unsigned long i = 0; i < VFS_MAX_NODES; i++)
    {
        if (!nodes[i].used)
        {
            return &nodes[i];
        }
    }
    return 0;
}

static void vfs_set_path(struct ramfs_node *node, const char *path)
{
    unsigned long i = 0;
    for (; path[i]; i++)
    {
        node->path[i] = path[i];
    }
    node->path[i] = '\0';
}

void vfs_init(void)
{
    file_count = 0;
    dir_count = 0;
    for (unsigned long i = 0; i < VFS_MAX_NODES; i++)
    {
        nodes[i].used = 0;
        nodes[i].size = 0;
        nodes[i].path[0] = '\0';
    }
}

int vfs_mkdir(const char *path)
{
    struct ramfs_node *node;

    if (!vfs_path_valid(path) || vfs_find(path) ||
        !vfs_parent_exists(path) || dir_count >= VFS_MAX_DIRS)
    {
        return 0;
    }
    node = vfs_alloc_node();
    if (!node)
    {
        return 0;
    }
    vfs_set_path(node, path);
    node->size = 0;
    node->kind = RAMFS_DIRECTORY;
    node->used = 1;
    dir_count++;
    return 1;
}

int vfs_create(const char *path, const void *data, unsigned long size)
{
    struct ramfs_node *file;
    const char *disk_path = vfs_disk_path(path);

    if (disk_path)
    {
        return sfs_write_file(disk_path, data, size);
    }

    if (!vfs_path_valid(path) || size > VFS_MAX_FILE_SIZE ||
        (size && !data) || vfs_find(path) || !vfs_parent_exists(path) ||
        file_count >= VFS_MAX_FILES)
    {
        return 0;
    }

    file = vfs_alloc_node();
    if (!file)
    {
        return 0;
    }

    vfs_set_path(file, path);
    unsigned long i = 0;
    for (i = 0; i < size; i++)
    {
        file->data[i] = ((const unsigned char *)data)[i];
    }
    file->size = size;
    file->kind = RAMFS_FILE;
    file->used = 1;
    file_count++;
    return 1;
}

long vfs_write(const char *path,
               unsigned long offset,
               const void *buffer,
               unsigned long size)
{
    const char *disk_path = vfs_disk_path(path);
    struct ramfs_node *file = vfs_find(path);

    if (disk_path)
    {
        long current;
        unsigned long final_size;

        if (!size || !buffer || offset > SFS_MAX_FILE_SIZE ||
            size > SFS_MAX_FILE_SIZE - offset)
        {
            return -1;
        }
        current = sfs_read_file(disk_path,
                                disk_file_buffer,
                                sizeof(disk_file_buffer));
        if (current < 0)
        {
            return -1;
        }
        for (unsigned long i = 0; i < size; i++)
        {
            disk_file_buffer[offset + i] =
                ((const unsigned char *)buffer)[i];
        }
        final_size = offset + size > (unsigned long)current ?
            offset + size : (unsigned long)current;
        return sfs_write_file(disk_path, disk_file_buffer, final_size) ?
            (long)size : -1;
    }

    if (!file || file->kind != RAMFS_FILE || (size && !buffer) ||
        offset > VFS_MAX_FILE_SIZE || size > VFS_MAX_FILE_SIZE - offset)
    {
        return -1;
    }
    for (unsigned long i = 0; i < size; i++)
    {
        file->data[offset + i] = ((const unsigned char *)buffer)[i];
    }
    if (offset + size > file->size)
    {
        file->size = offset + size;
    }
    return (long)size;
}

int vfs_truncate(const char *path, unsigned long size)
{
    const char *disk_path = vfs_disk_path(path);
    struct ramfs_node *file = vfs_find(path);

    if (disk_path)
    {
        long current;

        if (size > SFS_MAX_FILE_SIZE)
        {
            return 0;
        }
        current = sfs_read_file(disk_path,
                                disk_file_buffer,
                                sizeof(disk_file_buffer));
        if (current < 0)
        {
            return 0;
        }
        for (unsigned long i = (unsigned long)current; i < size; i++)
        {
            disk_file_buffer[i] = 0;
        }
        return sfs_write_file(disk_path, disk_file_buffer, size);
    }

    if (!file || file->kind != RAMFS_FILE || size > VFS_MAX_FILE_SIZE)
    {
        return 0;
    }
    if (size > file->size)
    {
        for (unsigned long i = file->size; i < size; i++)
        {
            file->data[i] = 0;
        }
    }
    file->size = size;
    return 1;
}

long vfs_read(const char *path,
              unsigned long offset,
              void *buffer,
              unsigned long size)
{
    const char *disk_path = vfs_disk_path(path);
    struct ramfs_node *file = vfs_find(path);

    if (disk_path)
    {
        long file_size = sfs_read_file(disk_path,
                                       disk_file_buffer,
                                       sizeof(disk_file_buffer));
        if ((size && !buffer) || file_size < 0 ||
            offset > (unsigned long)file_size)
        {
            return -1;
        }
        if (size > (unsigned long)file_size - offset)
        {
            size = (unsigned long)file_size - offset;
        }
        for (unsigned long i = 0; i < size; i++)
        {
            ((unsigned char *)buffer)[i] = disk_file_buffer[offset + i];
        }
        return (long)size;
    }

    if (!file || file->kind != RAMFS_FILE ||
        (size && !buffer) || offset > file->size)
    {
        return -1;
    }
    if (size > file->size - offset)
    {
        size = file->size - offset;
    }
    for (unsigned long i = 0; i < size; i++)
    {
        ((unsigned char *)buffer)[i] = file->data[offset + i];
    }
    return (long)size;
}

const void *vfs_file_data(const char *path, unsigned long *size)
{
    const char *disk_path = vfs_disk_path(path);
    struct ramfs_node *file = vfs_find(path);

    if (disk_path)
    {
        long file_size = sfs_read_file(disk_path,
                                       disk_file_buffer,
                                       sizeof(disk_file_buffer));
        if (file_size < 0)
        {
            return 0;
        }
        if (size)
        {
            *size = (unsigned long)file_size;
        }
        return disk_file_buffer;
    }

    if (!file || file->kind != RAMFS_FILE)
    {
        return 0;
    }
    if (size)
    {
        *size = file->size;
    }
    return file->data;
}

void vfs_dump_files(void)
{
    kprintf("RAM filesystem: dirs=%d files=%d capacity=%d\n",
            (int)dir_count,
            (int)file_count,
            (int)VFS_MAX_NODES);
    for (unsigned long i = 0; i < VFS_MAX_NODES; i++)
    {
        if (nodes[i].used)
        {
            kprintf("  %s%s size=%d\n",
                    nodes[i].path,
                    nodes[i].kind == RAMFS_DIRECTORY ? "/" : "",
                    (int)nodes[i].size);
        }
    }
    kprintf("Mount: /disk -> SaturnFS\n");
    sfs_dump();
}

unsigned long vfs_dir_count(void)
{
    return dir_count;
}

unsigned long vfs_file_count(void)
{
    return file_count;
}

int vfs_list(unsigned long index, struct vfs_entry *entry)
{
    unsigned long current = 0;

    if (!entry)
    {
        return -1;
    }
    for (unsigned long i = 0; i < VFS_MAX_NODES; i++)
    {
        if (!nodes[i].used)
        {
            continue;
        }
        if (current++ != index)
        {
            continue;
        }
        unsigned long j = 0;
        for (; nodes[i].path[j] && j + 1UL < sizeof(entry->path); j++)
        {
            entry->path[j] = nodes[i].path[j];
        }
        entry->path[j] = '\0';
        for (j++; j < sizeof(entry->path); j++)
        {
            entry->path[j] = '\0';
        }
        entry->size = nodes[i].size;
        entry->kind = nodes[i].kind == RAMFS_DIRECTORY ?
            VFS_ENTRY_DIRECTORY : VFS_ENTRY_FILE;
        return 1;
    }
    return 0;
}
