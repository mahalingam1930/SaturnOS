#include "framebuffer.h"

#define FW_CFG_BASE 0x09020000UL
#define FW_CFG_DATA 0x00
#define FW_CFG_SELECTOR 0x08
#define FW_CFG_DMA 0x10

#define FW_CFG_FILE_DIR 0x0019
#define FW_CFG_DMA_CTL_ERROR 0x01
#define FW_CFG_DMA_CTL_READ 0x02
#define FW_CFG_DMA_CTL_SELECT 0x08
#define FW_CFG_DMA_CTL_WRITE 0x10

#define FW_CFG_MAX_FILE_PATH 56
#define RAMFB_FILE "etc/ramfb"
#define RAMFB_FORMAT_XRGB8888 0x34325258U
#define FW_CFG_MAX_FILES 64
#define FW_CFG_DMA_POLL_LIMIT 1000000U
#define FW_CFG_DMA_ACCESS_BASE 0x46ffe000UL
#define RAMFB_CONFIG_BASE 0x46fff000UL
#define FRAMEBUFFER_BASE 0x47000000UL

struct fw_cfg_file
{
    uint32_t size;
    uint16_t select;
    uint16_t reserved;
    char name[FW_CFG_MAX_FILE_PATH];
} __attribute__((packed));

struct fw_cfg_dma_access
{
    uint32_t control;
    uint32_t length;
    uint64_t address;
} __attribute__((packed, aligned(8)));

struct ramfb_config
{
    uint64_t addr;
    uint32_t fourcc;
    uint32_t flags;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
} __attribute__((packed));

static volatile uint32_t *framebuffer = (volatile uint32_t *)FRAMEBUFFER_BASE;
static int framebuffer_ready;
static int framebuffer_status_code;

static uint16_t bswap16(uint16_t value)
{
    return (uint16_t)((value >> 8) | (value << 8));
}

static uint32_t bswap32(uint32_t value)
{
    return ((value & 0x000000ffU) << 24) |
           ((value & 0x0000ff00U) << 8) |
           ((value & 0x00ff0000U) >> 8) |
           ((value & 0xff000000U) >> 24);
}

static uint64_t bswap64(uint64_t value)
{
    return ((uint64_t)bswap32((uint32_t)value) << 32) |
           (uint64_t)bswap32((uint32_t)(value >> 32));
}

static int string_equals(const char *left, const char *right)
{
    while (*left && *right)
    {
        if (*left != *right)
        {
            return 0;
        }

        left++;
        right++;
    }

    return *left == *right;
}

static int fw_cfg_dma_transfer(uint16_t selector,
                               void *buffer,
                               uint32_t size,
                               uint32_t operation)
{
    volatile struct fw_cfg_dma_access *access =
        (volatile struct fw_cfg_dma_access *)FW_CFG_DMA_ACCESS_BASE;
    uint64_t access_addr;
    uint32_t polls = 0;

    access->control = bswap32(((uint32_t)selector << 16) |
                              FW_CFG_DMA_CTL_SELECT |
                              operation);
    access->length = bswap32(size);
    access->address = bswap64((uint64_t)buffer);

    __asm__ volatile("dsb sy" ::: "memory");

    access_addr = (uint64_t)access;
    *(volatile uint32_t *)(FW_CFG_BASE + FW_CFG_DMA) =
        bswap32((uint32_t)(access_addr >> 32));
    *(volatile uint32_t *)(FW_CFG_BASE + FW_CFG_DMA + 4) =
        bswap32((uint32_t)access_addr);

    while ((bswap32(access->control) & ~FW_CFG_DMA_CTL_ERROR) &&
           polls < FW_CFG_DMA_POLL_LIMIT)
    {
        polls++;
    }

    if (polls == FW_CFG_DMA_POLL_LIMIT)
    {
        framebuffer_status_code = 2;
        return 0;
    }

    if (bswap32(access->control) & FW_CFG_DMA_CTL_ERROR)
    {
        framebuffer_status_code = 3;
        return 0;
    }

    return 1;
}

static int fw_cfg_dma_write(uint16_t selector, void *buffer, uint32_t size)
{
    return fw_cfg_dma_transfer(selector, buffer, size, FW_CFG_DMA_CTL_WRITE);
}

static int fw_cfg_dma_read(uint16_t selector, void *buffer, uint32_t size)
{
    return fw_cfg_dma_transfer(selector, buffer, size, FW_CFG_DMA_CTL_READ);
}

static uint16_t fw_cfg_find_file(const char *name)
{
    static uint8_t directory[sizeof(uint32_t) +
                             (FW_CFG_MAX_FILES * sizeof(struct fw_cfg_file))]
        __attribute__((aligned(8)));
    uint32_t directory_size;
    uint32_t count;

    if (!fw_cfg_dma_read(FW_CFG_FILE_DIR, directory, sizeof(uint32_t)))
    {
        framebuffer_status_code = 4;
        return 0;
    }

    count = bswap32(*(uint32_t *)directory);
    if (count > FW_CFG_MAX_FILES)
    {
        count = FW_CFG_MAX_FILES;
    }

    directory_size = sizeof(uint32_t) + (count * sizeof(struct fw_cfg_file));
    if (!fw_cfg_dma_read(FW_CFG_FILE_DIR, directory, directory_size))
    {
        framebuffer_status_code = 5;
        return 0;
    }

    for (uint32_t i = 0; i < count; i++)
    {
        struct fw_cfg_file *file =
            (struct fw_cfg_file *)(directory + sizeof(uint32_t) +
                                   (i * sizeof(struct fw_cfg_file)));

        if (string_equals(file->name, name))
        {
            return bswap16(file->select);
        }
    }

    return 0;
}

int framebuffer_init(void)
{
    struct ramfb_config *config = (struct ramfb_config *)RAMFB_CONFIG_BASE;
    uint16_t ramfb_selector = fw_cfg_find_file(RAMFB_FILE);

    if (ramfb_selector == 0)
    {
        framebuffer_ready = 0;
        framebuffer_status_code = 1;
        return 0;
    }

    config->addr = bswap64((uint64_t)FRAMEBUFFER_BASE);
    config->fourcc = bswap32(RAMFB_FORMAT_XRGB8888);
    config->flags = 0;
    config->width = bswap32(FRAMEBUFFER_WIDTH);
    config->height = bswap32(FRAMEBUFFER_HEIGHT);
    config->stride = bswap32(FRAMEBUFFER_WIDTH * sizeof(uint32_t));

    framebuffer_ready = fw_cfg_dma_write(ramfb_selector,
                                         config,
                                         sizeof(struct ramfb_config));
    if (framebuffer_ready)
    {
        framebuffer_status_code = 0;
        return framebuffer_ready;
    }

    return framebuffer_ready;
}

int framebuffer_is_ready(void)
{
    return framebuffer_ready;
}

int framebuffer_status(void)
{
    return framebuffer_status_code;
}

void framebuffer_clear(uint32_t color)
{
    framebuffer_fill_rect(0, 0, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, color);
}

void framebuffer_put_pixel(unsigned int x, unsigned int y, uint32_t color)
{
    if (x >= FRAMEBUFFER_WIDTH || y >= FRAMEBUFFER_HEIGHT)
    {
        return;
    }

    framebuffer[y * FRAMEBUFFER_WIDTH + x] = color;
}

void framebuffer_fill_rect(unsigned int x,
                           unsigned int y,
                           unsigned int width,
                           unsigned int height,
                           uint32_t color)
{
    unsigned int x_end = x + width;
    unsigned int y_end = y + height;

    if (x >= FRAMEBUFFER_WIDTH || y >= FRAMEBUFFER_HEIGHT)
    {
        return;
    }

    if (x_end > FRAMEBUFFER_WIDTH)
    {
        x_end = FRAMEBUFFER_WIDTH;
    }

    if (y_end > FRAMEBUFFER_HEIGHT)
    {
        y_end = FRAMEBUFFER_HEIGHT;
    }

    for (unsigned int row = y; row < y_end; row++)
    {
        for (unsigned int col = x; col < x_end; col++)
        {
            framebuffer[row * FRAMEBUFFER_WIDTH + col] = color;
        }
    }
}

void framebuffer_draw_test_pattern(void)
{
    framebuffer_clear(0x00101820);
    framebuffer_fill_rect(48, 48, 544, 64, 0x00205070);
    framebuffer_fill_rect(48, 136, 160, 240, 0x00ff4f5e);
    framebuffer_fill_rect(240, 136, 160, 240, 0x004ecdc4);
    framebuffer_fill_rect(432, 136, 160, 240, 0x00ffd166);
    framebuffer_fill_rect(48, 408, 544, 24, 0x00f8f9fa);
}
