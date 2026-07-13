#include "keyboard.h"
#include "uart.h"

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

#define VIRTIO_MAGIC 0x74726976U
#define VIRTIO_VERSION_LEGACY 1U
#define VIRTIO_VERSION_MODERN 2U
#define VIRTIO_DEVICE_INPUT 18U
#define VIRTIO_F_VERSION_1_BIT 32U

#define VIRTIO_STATUS_ACKNOWLEDGE 1U
#define VIRTIO_STATUS_DRIVER 2U
#define VIRTIO_STATUS_DRIVER_OK 4U
#define VIRTIO_STATUS_FEATURES_OK 8U

#define VIRTQ_DESC_F_WRITE 2U
#define VIRTIO_KEYBOARD_QUEUE_SIZE 16U
#define VIRTIO_KEYBOARD_QUEUE_ALIGN 4096U
#define VIRTIO_KEYBOARD_QUEUE_BYTES 8192U

#define EV_KEY 0x01U
#define KEY_BACKSPACE 14U
#define KEY_TAB 15U
#define KEY_ENTER 28U
#define KEY_LEFTSHIFT 42U
#define KEY_RIGHTSHIFT 54U
#define KEY_SPACE 57U

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
    uint16_t ring[VIRTIO_KEYBOARD_QUEUE_SIZE];
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
    struct virtq_used_elem ring[VIRTIO_KEYBOARD_QUEUE_SIZE];
    uint16_t avail_event;
} __attribute__((packed));

struct virtio_input_event
{
    uint16_t type;
    uint16_t code;
    int32_t value;
} __attribute__((packed));

static uint8_t keyboard_queue[VIRTIO_KEYBOARD_QUEUE_BYTES]
    __attribute__((aligned(VIRTIO_KEYBOARD_QUEUE_ALIGN)));
static struct virtq_desc *keyboard_desc;
static struct virtq_avail *keyboard_avail;
static volatile struct virtq_used *keyboard_used;
static volatile struct virtio_input_event keyboard_events[VIRTIO_KEYBOARD_QUEUE_SIZE]
    __attribute__((aligned(8)));

static unsigned long virtio_keyboard_base;
static uint16_t virtio_keyboard_last_used;
static int virtio_keyboard_ready;
static int keyboard_initialized;
static int keyboard_shift;

static uint32_t mmio_read32(unsigned long address)
{
    return *(volatile uint32_t *)address;
}

static void mmio_write32(unsigned long address, uint32_t value)
{
    *(volatile uint32_t *)address = value;
}

static void memory_barrier(void)
{
    __asm__ volatile("dsb sy" ::: "memory");
}

static uint32_t virtio_read(unsigned long base, unsigned long offset)
{
    return mmio_read32(base + offset);
}

static void virtio_write(unsigned long base, unsigned long offset, uint32_t value)
{
    mmio_write32(base + offset, value);
}

static void virtio_keyboard_setup_queue_memory(void)
{
    for (unsigned int i = 0; i < VIRTIO_KEYBOARD_QUEUE_BYTES; i++)
    {
        keyboard_queue[i] = 0;
    }

    keyboard_desc = (struct virtq_desc *)keyboard_queue;
    keyboard_avail =
        (struct virtq_avail *)(keyboard_queue +
                               (sizeof(struct virtq_desc) *
                                VIRTIO_KEYBOARD_QUEUE_SIZE));
    keyboard_used =
        (volatile struct virtq_used *)(keyboard_queue +
                                       VIRTIO_KEYBOARD_QUEUE_ALIGN);
}

static void virtio_keyboard_add_buffer(uint16_t index)
{
    keyboard_desc[index].addr = (uint64_t)&keyboard_events[index];
    keyboard_desc[index].len = sizeof(struct virtio_input_event);
    keyboard_desc[index].flags = VIRTQ_DESC_F_WRITE;
    keyboard_desc[index].next = 0;

    keyboard_avail->ring[keyboard_avail->idx % VIRTIO_KEYBOARD_QUEUE_SIZE] =
        index;
    memory_barrier();
    keyboard_avail->idx++;
    memory_barrier();

    virtio_write(virtio_keyboard_base, VIRTIO_MMIO_QUEUE_NOTIFY, 0);
}

static void virtio_keyboard_clear_events(void)
{
    for (unsigned int i = 0; i < VIRTIO_KEYBOARD_QUEUE_SIZE; i++)
    {
        keyboard_events[i].type = 0;
        keyboard_events[i].code = 0;
        keyboard_events[i].value = 0;
    }
}

static int virtio_keyboard_finish_modern(unsigned long base, uint32_t status)
{
    uint32_t features_hi;
    uint32_t queue_max;

    virtio_write(base, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
    features_hi = virtio_read(base, VIRTIO_MMIO_DEVICE_FEATURES);
    if ((features_hi & (1U << (VIRTIO_F_VERSION_1_BIT - 32U))) == 0)
    {
        virtio_write(base, VIRTIO_MMIO_STATUS, 0);
        return 0;
    }

    virtio_write(base, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    virtio_write(base, VIRTIO_MMIO_DRIVER_FEATURES, 0);
    virtio_write(base, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
    virtio_write(base,
                 VIRTIO_MMIO_DRIVER_FEATURES,
                 1U << (VIRTIO_F_VERSION_1_BIT - 32U));

    status |= VIRTIO_STATUS_FEATURES_OK;
    virtio_write(base, VIRTIO_MMIO_STATUS, status);
    if ((virtio_read(base, VIRTIO_MMIO_STATUS) &
         VIRTIO_STATUS_FEATURES_OK) == 0)
    {
        virtio_write(base, VIRTIO_MMIO_STATUS, 0);
        return 0;
    }

    virtio_write(base, VIRTIO_MMIO_QUEUE_SEL, 0);
    queue_max = virtio_read(base, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (queue_max < VIRTIO_KEYBOARD_QUEUE_SIZE)
    {
        virtio_write(base, VIRTIO_MMIO_STATUS, 0);
        return 0;
    }

    virtio_keyboard_clear_events();
    virtio_keyboard_setup_queue_memory();
    virtio_keyboard_last_used = 0;
    virtio_keyboard_base = base;

    virtio_write(base, VIRTIO_MMIO_QUEUE_READY, 0);
    virtio_write(base, VIRTIO_MMIO_QUEUE_NUM, VIRTIO_KEYBOARD_QUEUE_SIZE);
    virtio_write(base,
                 VIRTIO_MMIO_QUEUE_DESC_LOW,
                 (uint32_t)((uint64_t)keyboard_desc));
    virtio_write(base,
                 VIRTIO_MMIO_QUEUE_DESC_HIGH,
                 (uint32_t)((uint64_t)keyboard_desc >> 32));
    virtio_write(base,
                 VIRTIO_MMIO_QUEUE_DRIVER_LOW,
                 (uint32_t)((uint64_t)keyboard_avail));
    virtio_write(base,
                 VIRTIO_MMIO_QUEUE_DRIVER_HIGH,
                 (uint32_t)((uint64_t)keyboard_avail >> 32));
    virtio_write(base,
                 VIRTIO_MMIO_QUEUE_DEVICE_LOW,
                 (uint32_t)((uint64_t)keyboard_used));
    virtio_write(base,
                 VIRTIO_MMIO_QUEUE_DEVICE_HIGH,
                 (uint32_t)((uint64_t)keyboard_used >> 32));
    virtio_write(base, VIRTIO_MMIO_QUEUE_READY, 1);

    for (uint16_t i = 0; i < VIRTIO_KEYBOARD_QUEUE_SIZE; i++)
    {
        virtio_keyboard_add_buffer(i);
    }

    status |= VIRTIO_STATUS_DRIVER_OK;
    virtio_write(base, VIRTIO_MMIO_STATUS, status);
    virtio_keyboard_ready = 1;
    return 1;
}

static int virtio_keyboard_finish_legacy(unsigned long base, uint32_t status)
{
    uint32_t queue_max;

    virtio_write(base, VIRTIO_MMIO_DRIVER_FEATURES, 0);
    virtio_write(base, VIRTIO_MMIO_GUEST_PAGE_SIZE, VIRTIO_KEYBOARD_QUEUE_ALIGN);

    virtio_write(base, VIRTIO_MMIO_QUEUE_SEL, 0);
    queue_max = virtio_read(base, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (queue_max < VIRTIO_KEYBOARD_QUEUE_SIZE)
    {
        virtio_write(base, VIRTIO_MMIO_STATUS, 0);
        return 0;
    }

    virtio_keyboard_clear_events();
    virtio_keyboard_setup_queue_memory();
    virtio_keyboard_last_used = 0;
    virtio_keyboard_base = base;

    virtio_write(base, VIRTIO_MMIO_QUEUE_NUM, VIRTIO_KEYBOARD_QUEUE_SIZE);
    virtio_write(base, VIRTIO_MMIO_QUEUE_ALIGN, VIRTIO_KEYBOARD_QUEUE_ALIGN);
    virtio_write(base,
                 VIRTIO_MMIO_QUEUE_PFN,
                 (uint32_t)((uint64_t)keyboard_queue >>
                            12));

    for (uint16_t i = 0; i < VIRTIO_KEYBOARD_QUEUE_SIZE; i++)
    {
        virtio_keyboard_add_buffer(i);
    }

    status |= VIRTIO_STATUS_DRIVER_OK;
    virtio_write(base, VIRTIO_MMIO_STATUS, status);
    virtio_keyboard_ready = 1;
    return 1;
}

static int virtio_keyboard_probe_transport(unsigned long base)
{
    uint32_t status;
    uint32_t version;

    if (virtio_read(base, VIRTIO_MMIO_MAGIC_VALUE) != VIRTIO_MAGIC ||
        virtio_read(base, VIRTIO_MMIO_DEVICE_ID) != VIRTIO_DEVICE_INPUT)
    {
        return 0;
    }

    version = virtio_read(base, VIRTIO_MMIO_VERSION);
    virtio_write(base, VIRTIO_MMIO_STATUS, 0);
    status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    virtio_write(base, VIRTIO_MMIO_STATUS, status);

    if (version == VIRTIO_VERSION_MODERN)
    {
        return virtio_keyboard_finish_modern(base, status);
    }

    if (version == VIRTIO_VERSION_LEGACY)
    {
        return virtio_keyboard_finish_legacy(base, status);
    }

    virtio_write(base, VIRTIO_MMIO_STATUS, 0);
    return 0;
}

static void virtio_keyboard_init(void)
{
    for (unsigned int i = 0; i < VIRTIO_MMIO_TRANSPORTS; i++)
    {
        unsigned long base = VIRTIO_MMIO_BASE + (i * VIRTIO_MMIO_STRIDE);

        if (virtio_keyboard_probe_transport(base))
        {
            return;
        }
    }
}

static char keycode_to_ascii(uint16_t code)
{
    static const char normal[] = {
        [2] = '1', [3] = '2', [4] = '3', [5] = '4', [6] = '5',
        [7] = '6', [8] = '7', [9] = '8', [10] = '9', [11] = '0',
        [12] = '-', [13] = '=',
        [16] = 'q', [17] = 'w', [18] = 'e', [19] = 'r', [20] = 't',
        [21] = 'y', [22] = 'u', [23] = 'i', [24] = 'o', [25] = 'p',
        [26] = '[', [27] = ']',
        [30] = 'a', [31] = 's', [32] = 'd', [33] = 'f', [34] = 'g',
        [35] = 'h', [36] = 'j', [37] = 'k', [38] = 'l', [39] = ';',
        [40] = '\'', [41] = '`', [43] = '\\',
        [44] = 'z', [45] = 'x', [46] = 'c', [47] = 'v', [48] = 'b',
        [49] = 'n', [50] = 'm', [51] = ',', [52] = '.', [53] = '/',
    };
    static const char shifted[] = {
        [2] = '!', [3] = '@', [4] = '#', [5] = '$', [6] = '%',
        [7] = '^', [8] = '&', [9] = '*', [10] = '(', [11] = ')',
        [12] = '_', [13] = '+',
        [16] = 'Q', [17] = 'W', [18] = 'E', [19] = 'R', [20] = 'T',
        [21] = 'Y', [22] = 'U', [23] = 'I', [24] = 'O', [25] = 'P',
        [26] = '{', [27] = '}',
        [30] = 'A', [31] = 'S', [32] = 'D', [33] = 'F', [34] = 'G',
        [35] = 'H', [36] = 'J', [37] = 'K', [38] = 'L', [39] = ':',
        [40] = '"', [41] = '~', [43] = '|',
        [44] = 'Z', [45] = 'X', [46] = 'C', [47] = 'V', [48] = 'B',
        [49] = 'N', [50] = 'M', [51] = '<', [52] = '>', [53] = '?',
    };

    if (code == KEY_ENTER)
    {
        return '\n';
    }

    if (code == KEY_BACKSPACE)
    {
        return '\b';
    }

    if (code == KEY_TAB)
    {
        return '\t';
    }

    if (code == KEY_SPACE)
    {
        return ' ';
    }

    if (code >= sizeof(normal))
    {
        return 0;
    }

    return keyboard_shift ? shifted[code] : normal[code];
}

static int virtio_keyboard_read_char(char *out)
{
    while (virtio_keyboard_ready &&
           virtio_keyboard_last_used != keyboard_used->idx)
    {
        uint16_t used_index =
            virtio_keyboard_last_used % VIRTIO_KEYBOARD_QUEUE_SIZE;
        uint32_t id = keyboard_used->ring[used_index].id;
        struct virtio_input_event event;
        char c;

        memory_barrier();
        virtio_keyboard_last_used++;

        if (id >= VIRTIO_KEYBOARD_QUEUE_SIZE)
        {
            continue;
        }

        event.type = keyboard_events[id].type;
        event.code = keyboard_events[id].code;
        event.value = keyboard_events[id].value;
        virtio_keyboard_add_buffer((uint16_t)id);

        if (event.type != EV_KEY)
        {
            continue;
        }

        if (event.code == KEY_LEFTSHIFT || event.code == KEY_RIGHTSHIFT)
        {
            keyboard_shift = event.value != 0;
            continue;
        }

        if (event.value == 0)
        {
            continue;
        }

        c = keycode_to_ascii(event.code);
        if (c)
        {
            *out = c;
            return 1;
        }
    }

    return 0;
}

void keyboard_init(void)
{
    if (keyboard_initialized)
    {
        return;
    }

    keyboard_initialized = 1;
    virtio_keyboard_init();
}

int keyboard_read_char(char *out)
{
    if (virtio_keyboard_read_char(out))
    {
        return 1;
    }

    if (!uart_read_ready())
    {
        return 0;
    }

    *out = uart_getc();
    return 1;
}

int keyboard_graphical_ready(void)
{
    return virtio_keyboard_ready;
}

const char *keyboard_source(void)
{
    if (virtio_keyboard_ready)
    {
        return "UART + virtio keyboard";
    }

    return "UART";
}
