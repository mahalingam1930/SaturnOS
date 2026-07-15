#include "virtio_blk.h"
#include <stdint.h>

#define VIRTIO_MMIO_BASE 0x0a000000UL
#define VIRTIO_MMIO_STRIDE 0x200UL
#define VIRTIO_MMIO_TRANSPORTS 32U
#define VIRTIO_MMIO_MAGIC_VALUE 0x000
#define VIRTIO_MMIO_VERSION 0x004
#define VIRTIO_MMIO_DEVICE_ID 0x008
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_GUEST_PAGE_SIZE 0x028
#define VIRTIO_MMIO_QUEUE_SEL 0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034
#define VIRTIO_MMIO_QUEUE_NUM 0x038
#define VIRTIO_MMIO_QUEUE_ALIGN 0x03c
#define VIRTIO_MMIO_QUEUE_PFN 0x040
#define VIRTIO_MMIO_QUEUE_READY 0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY 0x050
#define VIRTIO_MMIO_STATUS 0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW 0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_MMIO_QUEUE_DRIVER_LOW 0x090
#define VIRTIO_MMIO_QUEUE_DRIVER_HIGH 0x094
#define VIRTIO_MMIO_QUEUE_DEVICE_LOW 0x0a0
#define VIRTIO_MMIO_QUEUE_DEVICE_HIGH 0x0a4
#define VIRTIO_MMIO_CONFIG 0x100

#define VIRTIO_MAGIC 0x74726976U
#define VIRTIO_VERSION_MODERN 2U
#define VIRTIO_VERSION_LEGACY 1U
#define VIRTIO_DEVICE_BLOCK 2U
#define VIRTIO_F_VERSION_1_HIGH 1U
#define VIRTIO_STATUS_ACKNOWLEDGE 1U
#define VIRTIO_STATUS_DRIVER 2U
#define VIRTIO_STATUS_DRIVER_OK 4U
#define VIRTIO_STATUS_FEATURES_OK 8U
#define VIRTQ_DESC_F_NEXT 1U
#define VIRTQ_DESC_F_WRITE 2U
#define VIRTIO_BLK_T_IN 0U
#define VIRTIO_BLK_T_OUT 1U
#define VIRTIO_BLK_S_OK 0U
#define VIRTIO_BLK_QUEUE_SIZE 8U
#define VIRTIO_BLK_QUEUE_ALIGN 4096U
#define VIRTIO_BLK_QUEUE_BYTES 8192U
#define VIRTIO_BLK_SECTOR_SIZE 512UL
#define VIRTIO_BLK_TIMEOUT 10000000UL

struct virtq_desc
{
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct virtq_avail
{
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTIO_BLK_QUEUE_SIZE];
    uint16_t used_event;
} __attribute__((packed));

struct virtq_used_elem
{
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct virtq_used
{
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[VIRTIO_BLK_QUEUE_SIZE];
    uint16_t avail_event;
} __attribute__((packed));

struct virtio_blk_request
{
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed));

static uint8_t queue_memory[VIRTIO_BLK_QUEUE_BYTES]
    __attribute__((aligned(VIRTIO_BLK_QUEUE_ALIGN)));
static struct virtq_desc *descriptors;
static struct virtq_avail *available;
static volatile struct virtq_used *used;
static struct virtio_blk_request request __attribute__((aligned(8)));
static volatile uint8_t request_status;
static unsigned long device_base;
static unsigned long device_sectors;
static uint16_t last_used;
static int device_ready;

static uint32_t read32(unsigned long address)
{
    return *(volatile uint32_t *)address;
}

static void write32(unsigned long address, uint32_t value)
{
    *(volatile uint32_t *)address = value;
}

static void barrier(void)
{
    __asm__ volatile("dsb sy" ::: "memory");
}

static int virtio_blk_probe(unsigned long base)
{
    uint32_t status;
    uint32_t features_hi;
    uint32_t queue_max;
    uint32_t version;
    uint64_t address;

    if (read32(base + VIRTIO_MMIO_MAGIC_VALUE) != VIRTIO_MAGIC ||
        read32(base + VIRTIO_MMIO_DEVICE_ID) != VIRTIO_DEVICE_BLOCK)
    {
        return 0;
    }

    version = read32(base + VIRTIO_MMIO_VERSION);
    if (version != VIRTIO_VERSION_MODERN && version != VIRTIO_VERSION_LEGACY)
    {
        return 0;
    }
    write32(base + VIRTIO_MMIO_STATUS, 0);
    status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    write32(base + VIRTIO_MMIO_STATUS, status);
    if (version == VIRTIO_VERSION_MODERN)
    {
        write32(base + VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
        features_hi = read32(base + VIRTIO_MMIO_DEVICE_FEATURES);
        if (!(features_hi & VIRTIO_F_VERSION_1_HIGH))
        {
            write32(base + VIRTIO_MMIO_STATUS, 0);
            return 0;
        }
        write32(base + VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
        write32(base + VIRTIO_MMIO_DRIVER_FEATURES, 0);
        write32(base + VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
        write32(base + VIRTIO_MMIO_DRIVER_FEATURES,
                VIRTIO_F_VERSION_1_HIGH);
        status |= VIRTIO_STATUS_FEATURES_OK;
        write32(base + VIRTIO_MMIO_STATUS, status);
        if (!(read32(base + VIRTIO_MMIO_STATUS) &
              VIRTIO_STATUS_FEATURES_OK))
        {
            write32(base + VIRTIO_MMIO_STATUS, 0);
            return 0;
        }
    }
    else
    {
        write32(base + VIRTIO_MMIO_DRIVER_FEATURES, 0);
        write32(base + VIRTIO_MMIO_GUEST_PAGE_SIZE,
                VIRTIO_BLK_QUEUE_ALIGN);
    }

    write32(base + VIRTIO_MMIO_QUEUE_SEL, 0);
    queue_max = read32(base + VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (queue_max < VIRTIO_BLK_QUEUE_SIZE)
    {
        write32(base + VIRTIO_MMIO_STATUS, 0);
        return 0;
    }
    for (unsigned long i = 0; i < VIRTIO_BLK_QUEUE_BYTES; i++)
    {
        queue_memory[i] = 0;
    }
    descriptors = (struct virtq_desc *)queue_memory;
    available = (struct virtq_avail *)(queue_memory +
        sizeof(struct virtq_desc) * VIRTIO_BLK_QUEUE_SIZE);
    used = (volatile struct virtq_used *)(queue_memory +
                                          VIRTIO_BLK_QUEUE_ALIGN);
    last_used = 0;
    write32(base + VIRTIO_MMIO_QUEUE_NUM, VIRTIO_BLK_QUEUE_SIZE);
    if (version == VIRTIO_VERSION_MODERN)
    {
        write32(base + VIRTIO_MMIO_QUEUE_READY, 0);
        address = (uint64_t)descriptors;
        write32(base + VIRTIO_MMIO_QUEUE_DESC_LOW, (uint32_t)address);
        write32(base + VIRTIO_MMIO_QUEUE_DESC_HIGH,
                (uint32_t)(address >> 32));
        address = (uint64_t)available;
        write32(base + VIRTIO_MMIO_QUEUE_DRIVER_LOW, (uint32_t)address);
        write32(base + VIRTIO_MMIO_QUEUE_DRIVER_HIGH,
                (uint32_t)(address >> 32));
        address = (uint64_t)used;
        write32(base + VIRTIO_MMIO_QUEUE_DEVICE_LOW, (uint32_t)address);
        write32(base + VIRTIO_MMIO_QUEUE_DEVICE_HIGH,
                (uint32_t)(address >> 32));
        write32(base + VIRTIO_MMIO_QUEUE_READY, 1);
    }
    else
    {
        write32(base + VIRTIO_MMIO_QUEUE_ALIGN, VIRTIO_BLK_QUEUE_ALIGN);
        write32(base + VIRTIO_MMIO_QUEUE_PFN,
                (uint32_t)((uint64_t)queue_memory >> 12));
    }

    device_base = base;
    device_sectors = (unsigned long)read32(base + VIRTIO_MMIO_CONFIG) |
        ((unsigned long)read32(base + VIRTIO_MMIO_CONFIG + 4) << 32);
    status |= VIRTIO_STATUS_DRIVER_OK;
    write32(base + VIRTIO_MMIO_STATUS, status);
    device_ready = device_sectors > 0;
    return device_ready;
}

int virtio_blk_init(void)
{
    device_ready = 0;
    for (unsigned int i = 0; i < VIRTIO_MMIO_TRANSPORTS; i++)
    {
        if (virtio_blk_probe(VIRTIO_MMIO_BASE + i * VIRTIO_MMIO_STRIDE))
        {
            return 1;
        }
    }
    return 0;
}

static int virtio_blk_transfer(unsigned long sector,
                               void *buffer,
                               unsigned long count,
                               int write)
{
    uint16_t expected;

    if (!device_ready || !buffer || !count || sector >= device_sectors ||
        count > device_sectors - sector ||
        count > 0xffffffffUL / VIRTIO_BLK_SECTOR_SIZE)
    {
        return 0;
    }
    request.type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    request.reserved = 0;
    request.sector = sector;
    request_status = 0xffU;

    descriptors[0].addr = (uint64_t)&request;
    descriptors[0].len = sizeof(request);
    descriptors[0].flags = VIRTQ_DESC_F_NEXT;
    descriptors[0].next = 1;
    descriptors[1].addr = (uint64_t)buffer;
    descriptors[1].len = (uint32_t)(count * VIRTIO_BLK_SECTOR_SIZE);
    descriptors[1].flags = VIRTQ_DESC_F_NEXT |
        (write ? 0 : VIRTQ_DESC_F_WRITE);
    descriptors[1].next = 2;
    descriptors[2].addr = (uint64_t)&request_status;
    descriptors[2].len = 1;
    descriptors[2].flags = VIRTQ_DESC_F_WRITE;
    descriptors[2].next = 0;

    available->ring[available->idx % VIRTIO_BLK_QUEUE_SIZE] = 0;
    barrier();
    available->idx++;
    barrier();
    expected = last_used + 1U;
    write32(device_base + VIRTIO_MMIO_QUEUE_NOTIFY, 0);
    for (unsigned long timeout = 0; timeout < VIRTIO_BLK_TIMEOUT; timeout++)
    {
        barrier();
        if (used->idx == expected)
        {
            last_used = expected;
            return request_status == VIRTIO_BLK_S_OK;
        }
    }
    return 0;
}

int virtio_blk_read(unsigned long sector, void *buffer, unsigned long count)
{
    return virtio_blk_transfer(sector, buffer, count, 0);
}

int virtio_blk_write(unsigned long sector,
                     const void *buffer,
                     unsigned long count)
{
    return virtio_blk_transfer(sector, (void *)buffer, count, 1);
}

unsigned long virtio_blk_sector_count(void)
{
    return device_sectors;
}

int virtio_blk_ready(void)
{
    return device_ready;
}
