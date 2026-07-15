#ifndef VIRTIO_BLK_H
#define VIRTIO_BLK_H

int virtio_blk_init(void);
int virtio_blk_read(unsigned long sector, void *buffer, unsigned long count);
int virtio_blk_write(unsigned long sector,
                     const void *buffer,
                     unsigned long count);
unsigned long virtio_blk_sector_count(void);
int virtio_blk_ready(void);

#endif
