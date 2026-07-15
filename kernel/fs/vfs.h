#ifndef VFS_H
#define VFS_H

#define VFS_MAX_FILES 8UL
#define VFS_MAX_DIRS 8UL
#define VFS_MAX_NODES (VFS_MAX_FILES + VFS_MAX_DIRS)
#define VFS_MAX_PATH 48UL
#define VFS_MAX_FILE_SIZE 4096UL

void vfs_init(void);
int vfs_mkdir(const char *path);
int vfs_create(const char *path, const void *data, unsigned long size);
long vfs_write(const char *path,
               unsigned long offset,
               const void *buffer,
               unsigned long size);
int vfs_truncate(const char *path, unsigned long size);
long vfs_read(const char *path,
              unsigned long offset,
              void *buffer,
              unsigned long size);
const void *vfs_file_data(const char *path, unsigned long *size);
void vfs_dump_files(void);
unsigned long vfs_file_count(void);
unsigned long vfs_dir_count(void);

#endif
