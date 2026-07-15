#ifndef SFS_H
#define SFS_H

#define SFS_MAX_FILES 8UL
#define SFS_MAX_PATH 44UL
#define SFS_FILE_SECTORS 8UL
#define SFS_MAX_FILE_SIZE (SFS_FILE_SECTORS * 512UL)

void sfs_init(void);
int sfs_format(void);
int sfs_write_file(const char *path, const void *data, unsigned long size);
long sfs_read_file(const char *path, void *data, unsigned long capacity);
void sfs_dump(void);
int sfs_ready(void);

#endif
