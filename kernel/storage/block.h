#ifndef BLOCK_H
#define BLOCK_H

#define BLOCK_SECTOR_SIZE 512UL
#define BLOCK_RAM_SECTORS 128UL

void block_init(void);
int block_read(unsigned long sector, void *buffer, unsigned long count);
int block_write(unsigned long sector, const void *buffer, unsigned long count);
int block_self_test(void);
void block_dump_stats(void);
unsigned long block_sector_count(void);
const char *block_device_name(void);

#endif
