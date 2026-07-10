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

static const uint8_t glyph_blank[7] = {0, 0, 0, 0, 0, 0, 0};

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

static const uint8_t *framebuffer_glyph(char c)
{
    static const uint8_t glyph_0[7] = {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e};
    static const uint8_t glyph_1[7] = {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e};
    static const uint8_t glyph_2[7] = {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f};
    static const uint8_t glyph_3[7] = {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e};
    static const uint8_t glyph_4[7] = {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02};
    static const uint8_t glyph_5[7] = {0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e};
    static const uint8_t glyph_6[7] = {0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e};
    static const uint8_t glyph_7[7] = {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    static const uint8_t glyph_8[7] = {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e};
    static const uint8_t glyph_9[7] = {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e};
    static const uint8_t glyph_a[7] = {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11};
    static const uint8_t glyph_b[7] = {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e};
    static const uint8_t glyph_c[7] = {0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0f};
    static const uint8_t glyph_d[7] = {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e};
    static const uint8_t glyph_e[7] = {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f};
    static const uint8_t glyph_f[7] = {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10};
    static const uint8_t glyph_g[7] = {0x0f, 0x10, 0x10, 0x13, 0x11, 0x11, 0x0f};
    static const uint8_t glyph_h[7] = {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11};
    static const uint8_t glyph_i[7] = {0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e};
    static const uint8_t glyph_j[7] = {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0c};
    static const uint8_t glyph_k[7] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    static const uint8_t glyph_l[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f};
    static const uint8_t glyph_m[7] = {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11};
    static const uint8_t glyph_n[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
    static const uint8_t glyph_o[7] = {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e};
    static const uint8_t glyph_p[7] = {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10};
    static const uint8_t glyph_q[7] = {0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d};
    static const uint8_t glyph_r[7] = {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11};
    static const uint8_t glyph_s[7] = {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e};
    static const uint8_t glyph_t[7] = {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    static const uint8_t glyph_u[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e};
    static const uint8_t glyph_v[7] = {0x11, 0x11, 0x11, 0x11, 0x0a, 0x0a, 0x04};
    static const uint8_t glyph_w[7] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11};
    static const uint8_t glyph_x[7] = {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11};
    static const uint8_t glyph_y[7] = {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04};
    static const uint8_t glyph_z[7] = {0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f};
    static const uint8_t glyph_colon[7] = {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};
    static const uint8_t glyph_dash[7] = {0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00};
    static const uint8_t glyph_dot[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c};

    if (c >= 'a' && c <= 'z')
    {
        c = (char)(c - ('a' - 'A'));
    }

    switch (c)
    {
        case '0': return glyph_0;
        case '1': return glyph_1;
        case '2': return glyph_2;
        case '3': return glyph_3;
        case '4': return glyph_4;
        case '5': return glyph_5;
        case '6': return glyph_6;
        case '7': return glyph_7;
        case '8': return glyph_8;
        case '9': return glyph_9;
        case 'A': return glyph_a;
        case 'B': return glyph_b;
        case 'C': return glyph_c;
        case 'D': return glyph_d;
        case 'E': return glyph_e;
        case 'F': return glyph_f;
        case 'G': return glyph_g;
        case 'H': return glyph_h;
        case 'I': return glyph_i;
        case 'J': return glyph_j;
        case 'K': return glyph_k;
        case 'L': return glyph_l;
        case 'M': return glyph_m;
        case 'N': return glyph_n;
        case 'O': return glyph_o;
        case 'P': return glyph_p;
        case 'Q': return glyph_q;
        case 'R': return glyph_r;
        case 'S': return glyph_s;
        case 'T': return glyph_t;
        case 'U': return glyph_u;
        case 'V': return glyph_v;
        case 'W': return glyph_w;
        case 'X': return glyph_x;
        case 'Y': return glyph_y;
        case 'Z': return glyph_z;
        case ':': return glyph_colon;
        case '-': return glyph_dash;
        case '.': return glyph_dot;
        default: return glyph_blank;
    }
}

void framebuffer_draw_char(unsigned int x,
                           unsigned int y,
                           char c,
                           uint32_t foreground,
                           uint32_t background,
                           unsigned int scale)
{
    const uint8_t *glyph = framebuffer_glyph(c);

    if (scale == 0)
    {
        scale = 1;
    }

    framebuffer_fill_rect(x, y, 6 * scale, 8 * scale, background);

    for (unsigned int row = 0; row < 7; row++)
    {
        for (unsigned int col = 0; col < 5; col++)
        {
            if (glyph[row] & (1U << (4 - col)))
            {
                framebuffer_fill_rect(x + (col * scale),
                                      y + (row * scale),
                                      scale,
                                      scale,
                                      foreground);
            }
        }
    }
}

void framebuffer_write_at(unsigned int x,
                          unsigned int y,
                          const char *text,
                          uint32_t foreground,
                          uint32_t background,
                          unsigned int scale)
{
    unsigned int cursor_x = x;
    unsigned int cursor_y = y;
    unsigned int cell_width;
    unsigned int cell_height;

    if (scale == 0)
    {
        scale = 1;
    }

    cell_width = 6 * scale;
    cell_height = 8 * scale;

    while (*text)
    {
        if (*text == '\n')
        {
            cursor_x = x;
            cursor_y += cell_height;
        }
        else
        {
            framebuffer_draw_char(cursor_x,
                                  cursor_y,
                                  *text,
                                  foreground,
                                  background,
                                  scale);
            cursor_x += cell_width;
        }

        text++;
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
    framebuffer_write_at(72, 66, "SaturnOS", 0x00f8f9fa, 0x00205070, 3);
    framebuffer_write_at(72, 154, "RAMFB", 0x00101820, 0x00ff4f5e, 3);
    framebuffer_write_at(264, 154, "TEXT", 0x00101820, 0x004ecdc4, 3);
    framebuffer_write_at(456, 154, "0.4.0", 0x00101820, 0x00ffd166, 2);
    framebuffer_write_at(72,
                         414,
                         "FRAMEBUFFER TEXT ONLINE",
                         0x00101820,
                         0x00f8f9fa,
                         2);
}
