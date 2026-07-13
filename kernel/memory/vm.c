#include "vm.h"
#include "heap.h"
#include "kprintf.h"
#include "mmu.h"
#include "scheduler.h"

#define VM_L2_TABLE_CAPACITY 4UL
#define VM_L3_TABLE_CAPACITY 1UL
#define VM_UART_BASE 0x09000000UL
#define VM_UART_END 0x09001000UL
#define VM_GIC_BASE 0x08000000UL
#define VM_GIC_END 0x08020000UL
#define VM_FW_CFG_BASE 0x09020000UL
#define VM_FW_CFG_END 0x09021000UL
#define VM_RAMFB_CONTROL_BASE 0x46ffe000UL
#define VM_FRAMEBUFFER_BASE 0x47000000UL
#define VM_FRAMEBUFFER_END 0x4712c000UL
#define VM_KERNEL_L3_BASE 0x40000000UL
#define VM_KERNEL_L3_END (VM_KERNEL_L3_BASE + ARM64_L2_BLOCK_SIZE)
#define VM_NORMAL_XN_MEMORY_ATTR \
    (ARM64_NORMAL_MEMORY_ATTR | ARM64_DESC_PXN | ARM64_DESC_UXN)
#define VM_NORMAL_RO_XN_MEMORY_ATTR \
    ((ARM64_NORMAL_MEMORY_ATTR & ~ARM64_DESC_AP_MASK) | \
     ARM64_DESC_AP_RO_EL1 | \
     ARM64_DESC_PXN | \
     ARM64_DESC_UXN)
#define VM_NORMAL_RO_EXEC_MEMORY_ATTR \
    ((ARM64_NORMAL_MEMORY_ATTR & ~ARM64_DESC_AP_MASK) | \
     ARM64_DESC_AP_RO_EL1)

extern char _kernel_start[];
extern char _kernel_end[];
extern char _text_start[];
extern char _text_end[];
extern char _rodata_start[];
extern char _rodata_end[];
extern char _data_start[];
extern char _data_end[];
extern char _bss_start[];
extern char _bss_end[];

struct vm_region
{
    const char *name;
    unsigned long virtual_start;
    unsigned long physical_start;
    unsigned long size;
    unsigned long attributes;
    const char *type;
};

struct vm_l2_table_slot
{
    unsigned long l1_index;
    int used;
};

struct vm_named_region
{
    const char *name;
    unsigned long start;
    unsigned long end;
};

struct vm_permission_goal
{
    const char *name;
    unsigned long start;
    unsigned long end;
    int want_xn;
    int want_ro;
};

static const struct vm_region vm_regions[] = {
    {
        "mmio",
        0x08000000UL,
        0x08000000UL,
        0x02000000UL,
        ARM64_DEVICE_MEMORY_ATTR,
        "device",
    },
    {
        "ram",
        0x40000000UL,
        0x40000000UL,
        0x08000000UL,
        VM_NORMAL_XN_MEMORY_ATTR,
        "normal",
    },
};

static const struct vm_named_region vm_named_regions[] = {
    {"gic", VM_GIC_BASE, VM_GIC_END},
    {"uart", VM_UART_BASE, VM_UART_END},
    {"fw_cfg", VM_FW_CFG_BASE, VM_FW_CFG_END},
    {"kernel-text", (unsigned long)_text_start, (unsigned long)_text_end},
    {"kernel-rodata", (unsigned long)_rodata_start, (unsigned long)_rodata_end},
    {"kernel-data", (unsigned long)_data_start, (unsigned long)_data_end},
    {"kernel-bss", (unsigned long)_bss_start, (unsigned long)_bss_end},
    {"kernel", (unsigned long)_kernel_start, (unsigned long)_kernel_end},
    {"ramfb-control", VM_RAMFB_CONTROL_BASE, VM_FRAMEBUFFER_BASE},
    {"framebuffer", VM_FRAMEBUFFER_BASE, VM_FRAMEBUFFER_END},
};

static const struct vm_permission_goal vm_permission_goals[] = {
    {"text", (unsigned long)_text_start, (unsigned long)_text_end, 0, 1},
    {"rodata", (unsigned long)_rodata_start, (unsigned long)_rodata_end, 1, 1},
    {"data", (unsigned long)_data_start, (unsigned long)_data_end, 1, 0},
    {"bss", (unsigned long)_bss_start, (unsigned long)_bss_end, 1, 0},
    {"mmio", 0x08000000UL, 0x0a000000UL, 1, 0},
};

static unsigned long vm_l1_table[ARM64_TABLE_ENTRIES]
    __attribute__((aligned(ARM64_TABLE_SIZE)));
static unsigned long vm_l2_tables[VM_L2_TABLE_CAPACITY][ARM64_TABLE_ENTRIES]
    __attribute__((aligned(ARM64_TABLE_SIZE)));
static unsigned long vm_kernel_l3_table[ARM64_TABLE_ENTRIES]
    __attribute__((aligned(ARM64_TABLE_SIZE)));
static struct vm_l2_table_slot vm_l2_slots[VM_L2_TABLE_CAPACITY];
static int vm_ready;
static int vm_tables_built;
static int vm_validated;
static int vm_enable_attempted;
static int vm_kernel_l3_mapped;
static unsigned long vm_used_l2_tables;
static unsigned long vm_blocks_mapped;
static unsigned long vm_pages_mapped;
static unsigned long vm_guard_pages;
static unsigned long vm_blocks_validated;
static unsigned long vm_pages_validated;
static unsigned long vm_guard_pages_validated;
static unsigned long vm_validation_error_count;
static unsigned long vm_exec_blocks;
static unsigned long vm_xn_blocks;
static unsigned long vm_exec_pages;
static unsigned long vm_xn_pages;
static unsigned long vm_bytes_mapped;

static void clear_table(unsigned long *table)
{
    for (unsigned long i = 0; i < ARM64_TABLE_ENTRIES; i++)
    {
        table[i] = 0;
    }
}

static unsigned long align_down(unsigned long value, unsigned long size)
{
    return value & ~(size - 1);
}

static unsigned long align_up(unsigned long value, unsigned long size)
{
    return (value + size - 1) & ~(size - 1);
}

static int address_in_range(unsigned long address,
                            unsigned long start,
                            unsigned long end)
{
    return address >= start && address < end;
}

static int address_in_kernel_text(unsigned long address)
{
    unsigned long text_start =
        align_down((unsigned long)_text_start, ARM64_PAGE_SIZE);
    unsigned long text_end =
        align_up((unsigned long)_text_end, ARM64_PAGE_SIZE);

    return address_in_range(address, text_start, text_end);
}

static int address_in_kernel_rodata(unsigned long address)
{
    unsigned long rodata_start =
        align_down((unsigned long)_rodata_start, ARM64_PAGE_SIZE);
    unsigned long rodata_end =
        align_up((unsigned long)_rodata_end, ARM64_PAGE_SIZE);

    return address_in_range(address, rodata_start, rodata_end);
}

static unsigned long kernel_page_attributes(unsigned long address)
{
    if (address_in_kernel_text(address))
    {
        return VM_NORMAL_RO_EXEC_MEMORY_ATTR;
    }

    if (address_in_kernel_rodata(address))
    {
        return VM_NORMAL_RO_XN_MEMORY_ATTR;
    }

    return VM_NORMAL_XN_MEMORY_ATTR;
}

static int vm_address_is_guard_page(unsigned long address)
{
    return scheduler_address_is_stack_guard(address);
}

static unsigned long *get_l2_table(unsigned long l1_index)
{
    unsigned long slot;

    for (slot = 0; slot < vm_used_l2_tables; slot++)
    {
        if (vm_l2_slots[slot].used &&
            vm_l2_slots[slot].l1_index == l1_index)
        {
            return vm_l2_tables[slot];
        }
    }

    if (vm_used_l2_tables >= VM_L2_TABLE_CAPACITY)
    {
        return 0;
    }

    slot = vm_used_l2_tables++;
    vm_l2_slots[slot].used = 1;
    vm_l2_slots[slot].l1_index = l1_index;
    clear_table(vm_l2_tables[slot]);

    vm_l1_table[l1_index] =
        arm64_mmu_table_desc((unsigned long)vm_l2_tables[slot]);

    return vm_l2_tables[slot];
}

static int region_is_l2_aligned(const struct vm_region *region)
{
    return ((region->virtual_start & (ARM64_L2_BLOCK_SIZE - 1)) == 0) &&
           ((region->physical_start & (ARM64_L2_BLOCK_SIZE - 1)) == 0) &&
           ((region->size & (ARM64_L2_BLOCK_SIZE - 1)) == 0);
}

static int map_region(const struct vm_region *region)
{
    unsigned long va = region->virtual_start;
    unsigned long pa = region->physical_start;
    unsigned long remaining = region->size;

    if (!region_is_l2_aligned(region))
    {
        return 0;
    }

    while (remaining > 0)
    {
        unsigned long l1_index = arm64_mmu_l1_index(va);
        unsigned long l2_index = arm64_mmu_l2_index(va);
        unsigned long *l2_table = get_l2_table(l1_index);

        if (!l2_table)
        {
            return 0;
        }

        l2_table[l2_index] =
            arm64_mmu_l2_block_desc(pa, region->attributes);

        if (arm64_mmu_desc_is_execute_never(l2_table[l2_index]))
        {
            vm_xn_blocks++;
        }
        else
        {
            vm_exec_blocks++;
        }

        va += ARM64_L2_BLOCK_SIZE;
        pa += ARM64_L2_BLOCK_SIZE;
        remaining -= ARM64_L2_BLOCK_SIZE;
        vm_blocks_mapped++;
        vm_bytes_mapped += ARM64_L2_BLOCK_SIZE;
    }

    return 1;
}

static int map_kernel_l3_block(void)
{
    unsigned long l1_index = arm64_mmu_l1_index(VM_KERNEL_L3_BASE);
    unsigned long l2_index = arm64_mmu_l2_index(VM_KERNEL_L3_BASE);
    unsigned long *l2_table = get_l2_table(l1_index);

    if (!l2_table)
    {
        return 0;
    }

    clear_table(vm_kernel_l3_table);

    for (unsigned long i = 0; i < ARM64_TABLE_ENTRIES; i++)
    {
        unsigned long address = VM_KERNEL_L3_BASE + (i * ARM64_PAGE_SIZE);
        unsigned long attributes = kernel_page_attributes(address);

        if (vm_address_is_guard_page(address))
        {
            vm_guard_pages++;
            continue;
        }

        vm_kernel_l3_table[i] =
            arm64_mmu_l3_page_desc(address, attributes);

        if (arm64_mmu_desc_is_execute_never(vm_kernel_l3_table[i]))
        {
            vm_xn_pages++;
        }
        else
        {
            vm_exec_pages++;
        }

        vm_pages_mapped++;
        vm_bytes_mapped += ARM64_PAGE_SIZE;
    }

    if (arm64_mmu_desc_is_l2_block(l2_table[l2_index]))
    {
        if (arm64_mmu_desc_is_execute_never(l2_table[l2_index]))
        {
            vm_xn_blocks--;
        }
        else
        {
            vm_exec_blocks--;
        }

        vm_blocks_mapped--;
        vm_bytes_mapped -= ARM64_L2_BLOCK_SIZE;
    }

    l2_table[l2_index] =
        arm64_mmu_table_desc((unsigned long)vm_kernel_l3_table);

    vm_kernel_l3_mapped = 1;
    return 1;
}

static int build_tables(void)
{
    clear_table(vm_l1_table);
    clear_table(vm_kernel_l3_table);

    for (unsigned long i = 0; i < VM_L2_TABLE_CAPACITY; i++)
    {
        vm_l2_slots[i].l1_index = 0;
        vm_l2_slots[i].used = 0;
        clear_table(vm_l2_tables[i]);
    }

    vm_used_l2_tables = 0;
    vm_blocks_mapped = 0;
    vm_pages_mapped = 0;
    vm_guard_pages = 0;
    vm_blocks_validated = 0;
    vm_pages_validated = 0;
    vm_guard_pages_validated = 0;
    vm_validation_error_count = 0;
    vm_exec_blocks = 0;
    vm_xn_blocks = 0;
    vm_exec_pages = 0;
    vm_xn_pages = 0;
    vm_bytes_mapped = 0;
    vm_kernel_l3_mapped = 0;

    for (unsigned long i = 0; i < vm_region_count(); i++)
    {
        if (!map_region(&vm_regions[i]))
        {
            return 0;
        }
    }

    if (!map_kernel_l3_block())
    {
        return 0;
    }

    return 1;
}

static unsigned long *find_l2_table(unsigned long l1_index)
{
    for (unsigned long slot = 0; slot < vm_used_l2_tables; slot++)
    {
        if (vm_l2_slots[slot].used &&
            vm_l2_slots[slot].l1_index == l1_index)
        {
            return vm_l2_tables[slot];
        }
    }

    return 0;
}

static int translation_for_address(unsigned long address,
                                   unsigned long *physical,
                                   unsigned long *descriptor,
                                   unsigned long *level)
{
    unsigned long l1_index = arm64_mmu_l1_index(address);
    unsigned long l2_index = arm64_mmu_l2_index(address);
    unsigned long l3_index = arm64_mmu_l3_index(address);
    unsigned long l2_offset = address & (ARM64_L2_BLOCK_SIZE - 1);
    unsigned long l3_offset = address & (ARM64_PAGE_SIZE - 1);
    unsigned long l1_descriptor = vm_l1_table[l1_index];
    unsigned long *l2_table;
    unsigned long l2_descriptor;
    unsigned long *l3_table;
    unsigned long l3_descriptor;

    if (!arm64_mmu_desc_is_table(l1_descriptor))
    {
        return 0;
    }

    l2_table = find_l2_table(l1_index);
    if (!l2_table)
    {
        return 0;
    }

    l2_descriptor = l2_table[l2_index];
    if (arm64_mmu_desc_is_l2_block(l2_descriptor))
    {
        *physical = arm64_mmu_l2_block_address(l2_descriptor) + l2_offset;
        *descriptor = l2_descriptor;
        *level = 2;
        return 1;
    }

    if (!arm64_mmu_desc_is_table(l2_descriptor))
    {
        return 0;
    }

    l3_table = (unsigned long *)arm64_mmu_desc_address(l2_descriptor);
    l3_descriptor = l3_table[l3_index];

    if (!arm64_mmu_desc_is_l3_page(l3_descriptor))
    {
        return 0;
    }

    *physical = arm64_mmu_l3_page_address(l3_descriptor) + l3_offset;
    *descriptor = l3_descriptor;
    *level = 3;
    return 1;
}

static void validation_error(void)
{
    vm_validation_error_count++;
}

static int validate_region(const struct vm_region *region)
{
    unsigned long va = region->virtual_start;
    unsigned long pa = region->physical_start;
    unsigned long remaining = region->size;

    if (!region_is_l2_aligned(region))
    {
        validation_error();
        return 0;
    }

    while (remaining > 0)
    {
        unsigned long physical;
        unsigned long descriptor;
        unsigned long level;
        unsigned long step;

        if (vm_address_is_guard_page(va))
        {
            if (translation_for_address(va, &physical, &descriptor, &level))
            {
                validation_error();
                return 0;
            }

            va += ARM64_PAGE_SIZE;
            pa += ARM64_PAGE_SIZE;
            remaining -= ARM64_PAGE_SIZE;
            vm_guard_pages_validated++;
            continue;
        }

        if (!translation_for_address(va, &physical, &descriptor, &level))
        {
            validation_error();
            return 0;
        }

        if (physical != pa)
        {
            validation_error();
            return 0;
        }

        if (level == 2)
        {
            step = ARM64_L2_BLOCK_SIZE;
            vm_blocks_validated++;
        }
        else
        {
            step = ARM64_PAGE_SIZE;
            vm_pages_validated++;
        }

        va += step;
        pa += step;
        remaining -= step;
    }

    return 1;
}

static int validate_tables(void)
{
    vm_blocks_validated = 0;
    vm_pages_validated = 0;
    vm_validation_error_count = 0;

    if (((unsigned long)vm_l1_table & (ARM64_TABLE_SIZE - 1)) != 0)
    {
        validation_error();
        return 0;
    }

    for (unsigned long slot = 0; slot < vm_used_l2_tables; slot++)
    {
        if (((unsigned long)vm_l2_tables[slot] & (ARM64_TABLE_SIZE - 1)) != 0)
        {
            validation_error();
            return 0;
        }
    }

    if (vm_kernel_l3_mapped &&
        ((unsigned long)vm_kernel_l3_table & (ARM64_TABLE_SIZE - 1)) != 0)
    {
        validation_error();
        return 0;
    }

    for (unsigned long i = 0; i < vm_region_count(); i++)
    {
        if (!validate_region(&vm_regions[i]))
        {
            return 0;
        }
    }

    if (vm_blocks_validated != vm_blocks_mapped)
    {
        validation_error();
        return 0;
    }

    if (vm_pages_validated != vm_pages_mapped)
    {
        validation_error();
        return 0;
    }

    if (vm_guard_pages_validated != vm_guard_pages)
    {
        validation_error();
        return 0;
    }

    return 1;
}

void vm_init(void)
{
    vm_tables_built = build_tables();
    vm_validated = 0;
    vm_ready = 0;

    if (vm_tables_built)
    {
        vm_validated = validate_tables();
        vm_ready = vm_validated;
    }

    if (vm_ready)
    {
        arm64_mmu_enable(vm_root_table());
        vm_enable_attempted = 1;
    }
}

unsigned long vm_region_count(void)
{
    return sizeof(vm_regions) / sizeof(vm_regions[0]);
}

const char *vm_state(void)
{
    if (!vm_ready)
    {
        return "not initialized";
    }

    return arm64_mmu_state();
}

int vm_mmu_enabled(void)
{
    return arm64_mmu_is_enabled();
}

const char *vm_table_state(void)
{
    if (vm_tables_built)
    {
        return "built";
    }

    return "error";
}

const char *vm_validation_state(void)
{
    if (vm_validated)
    {
        return "ok";
    }

    return "failed";
}

unsigned long vm_table_count(void)
{
    if (!vm_tables_built)
    {
        return 0;
    }

    return 1 + vm_used_l2_tables + (vm_kernel_l3_mapped ? 1 : 0);
}

unsigned long vm_mapped_blocks(void)
{
    return vm_blocks_mapped;
}

unsigned long vm_validated_blocks(void)
{
    return vm_blocks_validated;
}

unsigned long vm_validation_errors(void)
{
    return vm_validation_error_count;
}

unsigned long vm_executable_blocks(void)
{
    return vm_exec_blocks;
}

unsigned long vm_execute_never_blocks(void)
{
    return vm_xn_blocks;
}

unsigned long vm_mapped_bytes(void)
{
    return vm_bytes_mapped;
}

unsigned long vm_root_table(void)
{
    return (unsigned long)vm_l1_table;
}

const char *vm_region_name_for_address(unsigned long address)
{
    unsigned long heap_start = heap_region_start();
    unsigned long heap_end = heap_region_end();
    unsigned long stack_start = scheduler_stack_region_start();
    unsigned long stack_end = scheduler_stack_region_end();

    if (heap_start != heap_end &&
        address >= heap_start &&
        address < heap_end)
    {
        return "heap";
    }

    if (scheduler_address_is_stack_guard(address))
    {
        return "stack-guard";
    }

    if (address >= stack_start && address < stack_end)
    {
        return "scheduler-stacks";
    }

    for (unsigned long i = 0;
         i < sizeof(vm_named_regions) / sizeof(vm_named_regions[0]);
         i++)
    {
        if (address >= vm_named_regions[i].start &&
            address < vm_named_regions[i].end)
        {
            return vm_named_regions[i].name;
        }
    }

    for (unsigned long i = 0; i < vm_region_count(); i++)
    {
        if (address >= vm_regions[i].virtual_start &&
            address < vm_regions[i].virtual_start + vm_regions[i].size)
        {
            return vm_regions[i].name;
        }
    }

    return "gap";
}

int vm_walk_address(unsigned long virtual_address, unsigned long *physical)
{
    unsigned long descriptor;
    unsigned long level;

    if (!translation_for_address(virtual_address,
                                 physical,
                                 &descriptor,
                                 &level))
    {
        return 0;
    }

    return 1;
}

void vm_dump_walk_address(const char *label, unsigned long address)
{
    unsigned long l1 = arm64_mmu_l1_index(address);
    unsigned long l2 = arm64_mmu_l2_index(address);
    unsigned long l3 = arm64_mmu_l3_index(address);
    unsigned long offset = address & (ARM64_L2_BLOCK_SIZE - 1);
    unsigned long physical = 0;
    unsigned long l1_descriptor = vm_l1_table[l1];
    unsigned long *l2_table;
    unsigned long l2_descriptor = 0;
    const char *region = vm_region_name_for_address(address);

    kprintf("VM walk %s: va=0x%x region=%s l1=%d l2=%d l3=%d off=0x%x\n",
            label,
            (unsigned int)address,
            region,
            (int)l1,
            (int)l2,
            (int)l3,
            (unsigned int)offset);
    kprintf("  l1 desc=0x%x %s\n",
            (unsigned int)l1_descriptor,
            arm64_mmu_desc_is_table(l1_descriptor) ? "table" : "invalid");

    l2_table = find_l2_table(l1);
    if (l2_table)
    {
        l2_descriptor = l2_table[l2];
        kprintf("  l2 desc=0x%x %s\n",
                (unsigned int)l2_descriptor,
                arm64_mmu_desc_is_l2_block(l2_descriptor) ? "block" :
                (arm64_mmu_desc_is_table(l2_descriptor) ? "table" :
                 "invalid"));
    }

    if (vm_walk_address(address, &physical))
    {
        unsigned long descriptor;
        unsigned long level;

        translation_for_address(address, &physical, &descriptor, &level);

        if (level == 3)
        {
            kprintf("  l3 desc=0x%x page perm=%s/%s\n",
                    (unsigned int)descriptor,
                    arm64_mmu_desc_execute_state(descriptor),
                    arm64_mmu_desc_write_state(descriptor));
        }
        else
        {
            kprintf("  final desc=0x%x block perm=%s/%s\n",
                    (unsigned int)descriptor,
                    arm64_mmu_desc_execute_state(descriptor),
                    arm64_mmu_desc_write_state(descriptor));
        }

        kprintf("  pa=0x%x\n", (unsigned int)physical);
    }
    else
    {
        kprintf("  not mapped\n");
    }
}

static void vm_dump_section_walk(const char *label, char *start, char *end)
{
    if ((unsigned long)start >= (unsigned long)end)
    {
        kprintf("VM walk %s: empty section\n", label);
        return;
    }

    vm_dump_walk_address(label, (unsigned long)start);
}

void vm_dump_walk_examples(void)
{
    unsigned long heap_start = heap_region_start();
    unsigned long guard_start = scheduler_stack_guard_start(0);

    vm_dump_walk_address("uart", 0x09000000UL);
    vm_dump_section_walk("text", _text_start, _text_end);
    vm_dump_section_walk("rodata", _rodata_start, _rodata_end);
    vm_dump_section_walk("data", _data_start, _data_end);
    vm_dump_section_walk("bss", _bss_start, _bss_end);
    if (heap_start != heap_region_end())
    {
        vm_dump_walk_address("heap", heap_start);
    }
    vm_dump_walk_address("stack-guard", guard_start);
    vm_dump_walk_address("stacks", scheduler_stack_start(0));
    vm_dump_walk_address("gap", 0x20000000UL);
}

static void vm_dump_kernel_sections(void)
{
    kprintf("VM kernel sections:\n");
    kprintf("  text   0x%x-0x%x\n",
            (unsigned int)(unsigned long)_text_start,
            (unsigned int)(unsigned long)_text_end);
    kprintf("  rodata 0x%x-0x%x\n",
            (unsigned int)(unsigned long)_rodata_start,
            (unsigned int)(unsigned long)_rodata_end);
    kprintf("  data   0x%x-0x%x\n",
            (unsigned int)(unsigned long)_data_start,
            (unsigned int)(unsigned long)_data_end);
    kprintf("  bss    0x%x-0x%x\n",
            (unsigned int)(unsigned long)_bss_start,
            (unsigned int)(unsigned long)_bss_end);
}

static void vm_dump_range(const char *name,
                          unsigned long start,
                          unsigned long end)
{
    if (start >= end)
    {
        kprintf("  %s empty\n", name);
        return;
    }

    kprintf("  %s 0x%x-0x%x\n",
            name,
            (unsigned int)start,
            (unsigned int)end);
}

static void vm_dump_named_ranges(void)
{
    unsigned long heap_start = heap_region_start();
    unsigned long heap_end = heap_region_end();

    kprintf("VM ranges:\n");
    vm_dump_range("kernel.text ",
                  (unsigned long)_text_start,
                  (unsigned long)_text_end);
    vm_dump_range("kernel.ro   ",
                  (unsigned long)_rodata_start,
                  (unsigned long)_rodata_end);
    vm_dump_range("kernel.bss  ",
                  (unsigned long)_bss_start,
                  (unsigned long)_bss_end);
    vm_dump_range("heap        ", heap_start, heap_end);
    vm_dump_range("stack0      ",
                  scheduler_stack_start(0),
                  scheduler_stack_end(0));
    vm_dump_range("stack.area  ",
                  scheduler_stack_guard_start(0),
                  scheduler_stack_guard_end(scheduler_stack_count()));
    vm_dump_range("guard.first ",
                  scheduler_stack_guard_start(0),
                  scheduler_stack_guard_end(0));
    vm_dump_range("guard.last  ",
                  scheduler_stack_guard_start(scheduler_stack_count()),
                  scheduler_stack_guard_end(scheduler_stack_count()));
    vm_dump_range("framebuffer ", VM_FRAMEBUFFER_BASE, VM_FRAMEBUFFER_END);
    vm_dump_range("gic         ", VM_GIC_BASE, VM_GIC_END);
    vm_dump_range("uart        ", VM_UART_BASE, VM_UART_END);
    vm_dump_range("fw_cfg      ", VM_FW_CFG_BASE, VM_FW_CFG_END);
}

static const char *vm_desired_permission(int want_xn)
{
    if (want_xn)
    {
        return "xn";
    }

    return "exec";
}

static const char *vm_desired_write_permission(int want_ro)
{
    if (want_ro)
    {
        return "ro";
    }

    return "rw";
}

static int next_descriptor_in_range(unsigned long *address,
                                    unsigned long end,
                                    unsigned long *descriptor)
{
    unsigned long physical;
    unsigned long level;
    unsigned long step;

    if (!translation_for_address(*address, &physical, descriptor, &level))
    {
        return 0;
    }

    step = (level == 2) ? ARM64_L2_BLOCK_SIZE : ARM64_PAGE_SIZE;
    *address = align_down(*address, step) + step;

    if (*address > end)
    {
        *address = end;
    }

    return 1;
}

static const char *vm_actual_execute_permission_for_range(unsigned long start,
                                                          unsigned long end)
{
    unsigned long address;
    int saw_exec = 0;
    int saw_xn = 0;

    if (start >= end)
    {
        return "empty";
    }

    address = align_down(start, ARM64_PAGE_SIZE);

    while (address < end)
    {
        unsigned long descriptor;

        if (vm_address_is_guard_page(address))
        {
            address += ARM64_PAGE_SIZE;
            continue;
        }

        if (!next_descriptor_in_range(&address, end, &descriptor))
        {
            return "unmapped";
        }

        if (arm64_mmu_desc_is_execute_never(descriptor))
        {
            saw_xn = 1;
        }
        else
        {
            saw_exec = 1;
        }

        if (saw_exec && saw_xn)
        {
            return "mixed";
        }
    }

    if (saw_xn)
    {
        return "xn";
    }

    if (!saw_exec)
    {
        return "unmapped";
    }

    return "exec";
}

static const char *vm_actual_write_permission_for_range(unsigned long start,
                                                        unsigned long end)
{
    unsigned long address;
    int saw_rw = 0;
    int saw_ro = 0;

    if (start >= end)
    {
        return "empty";
    }

    address = align_down(start, ARM64_PAGE_SIZE);

    while (address < end)
    {
        unsigned long descriptor;

        if (vm_address_is_guard_page(address))
        {
            address += ARM64_PAGE_SIZE;
            continue;
        }

        if (!next_descriptor_in_range(&address, end, &descriptor))
        {
            return "unmapped";
        }

        if (arm64_mmu_desc_is_read_only(descriptor))
        {
            saw_ro = 1;
        }
        else
        {
            saw_rw = 1;
        }

        if (saw_rw && saw_ro)
        {
            return "mixed";
        }
    }

    if (saw_ro)
    {
        return "ro";
    }

    if (!saw_rw)
    {
        return "unmapped";
    }

    return "rw";
}

static const char *vm_execute_permission_status(
    const struct vm_permission_goal *goal)
{
    const char *actual =
        vm_actual_execute_permission_for_range(goal->start, goal->end);

    if (actual[0] == 'e' && actual[1] == 'm')
    {
        return "empty";
    }

    if (actual[0] == 'u')
    {
        return "unmapped";
    }

    if (actual[0] == 'm')
    {
        return "mixed";
    }

    if (goal->want_xn)
    {
        if (actual[0] == 'x')
        {
            return "enforced";
        }

        return "pending-l3";
    }

    if (actual[0] == 'e')
    {
        return "enforced";
    }

    return "too-strict";
}

static const char *vm_write_permission_status(
    const struct vm_permission_goal *goal)
{
    const char *actual =
        vm_actual_write_permission_for_range(goal->start, goal->end);

    if (actual[0] == 'e')
    {
        return "empty";
    }

    if (actual[0] == 'u')
    {
        return "unmapped";
    }

    if (actual[0] == 'm')
    {
        return "mixed";
    }

    if (goal->want_ro)
    {
        if (actual[0] == 'r' && actual[1] == 'o')
        {
            return "enforced";
        }

        return "writable";
    }

    if (actual[0] == 'r' && actual[1] == 'w')
    {
        return "enforced";
    }

    return "too-strict";
}

static void vm_dump_permission_range(const char *name,
                                     unsigned long start,
                                     unsigned long end,
                                     int want_xn,
                                     int want_ro)
{
    kprintf("  %s exec=%s/%s %s write=%s/%s %s\n",
            name,
            vm_desired_permission(want_xn),
            vm_actual_execute_permission_for_range(start, end),
            vm_execute_permission_status(&(struct vm_permission_goal){
                name,
                start,
                end,
                want_xn,
                want_ro,
            }),
            vm_desired_write_permission(want_ro),
            vm_actual_write_permission_for_range(start, end),
            vm_write_permission_status(&(struct vm_permission_goal){
                name,
                start,
                end,
                want_xn,
                want_ro,
            }));
}

static const char *vm_stack_guard_state(void)
{
    for (unsigned long i = 0; i <= scheduler_stack_count(); i++)
    {
        unsigned long start = scheduler_stack_guard_start(i);
        unsigned long end = scheduler_stack_guard_end(i);

        for (unsigned long address = start;
             address < end;
             address += ARM64_PAGE_SIZE)
        {
            unsigned long physical;
            unsigned long descriptor;
            unsigned long level;

            if (translation_for_address(address,
                                        &physical,
                                        &descriptor,
                                        &level))
            {
                return "mapped";
            }
        }
    }

    return "unmapped";
}

static void vm_dump_stack_guard_state(void)
{
    const char *state = vm_stack_guard_state();

    kprintf("  guards pages=%d actual=%s status=%s\n",
            (int)vm_guard_pages,
            state,
            state[0] == 'u' ? "enforced" : "broken");
}

static void vm_dump_permission_goal(const struct vm_permission_goal *goal)
{
    vm_dump_permission_range(goal->name,
                             goal->start,
                             goal->end,
                             goal->want_xn,
                             goal->want_ro);
}

static void vm_dump_permission_goals(void)
{
    unsigned long heap_start = heap_region_start();
    unsigned long heap_end = heap_region_end();

    kprintf("VM protect: granularity=4 KiB kernel pages\n");

    for (unsigned long i = 0;
         i < sizeof(vm_permission_goals) / sizeof(vm_permission_goals[0]);
         i++)
    {
        vm_dump_permission_goal(&vm_permission_goals[i]);
    }

    vm_dump_permission_range("heap", heap_start, heap_end, 1, 0);
    vm_dump_permission_range("stacks",
                             scheduler_stack_region_start(),
                             scheduler_stack_region_end(),
                             1,
                             0);
    vm_dump_stack_guard_state();
}

static void vm_dump_security_summary(void)
{
    kprintf("VM security:\n");
    kprintf("  code     exec/ro\n");
    kprintf("  rodata   xn/ro\n");
    kprintf("  data     xn/rw\n");
    kprintf("  heap     xn/rw\n");
    kprintf("  stacks   xn/rw\n");
    kprintf("  guards   unmapped\n");
    kprintf("  mmio     xn/rw\n");
}

static int range_overlaps_kernel_l3_block(unsigned long start,
                                          unsigned long end)
{
    return vm_kernel_l3_mapped &&
           start < VM_KERNEL_L3_END &&
           end > VM_KERNEL_L3_BASE;
}

static void vm_dump_region(const struct vm_region *region)
{
    unsigned long end = region->virtual_start + region->size;
    unsigned long l1 = arm64_mmu_l1_index(region->virtual_start);
    unsigned long l2 = arm64_mmu_l2_index(region->virtual_start);
    unsigned long descriptor =
        arm64_mmu_l2_block_desc(region->physical_start, region->attributes);
    const char *permission = arm64_mmu_desc_execute_state(descriptor);

    if (range_overlaps_kernel_l3_block(region->virtual_start, end))
    {
        permission = "mixed";
    }

    kprintf("  %s: va=0x%x-0x%x pa=0x%x type=%s perm=%s l1=%d l2=%d\n",
            region->name,
            (unsigned int)region->virtual_start,
            (unsigned int)end,
            (unsigned int)region->physical_start,
            region->type,
            permission,
            (int)l1,
            (int)l2);
}

void vm_dump_plan(void)
{
    kprintf("VM state : %s\n", vm_state());
    if (vm_mmu_enabled())
    {
        kprintf("VM mode  : identity map active\n");
    }
    else if (vm_enable_attempted)
    {
        kprintf("VM mode  : enable attempted, MMU still disabled\n");
    }
    else
    {
        kprintf("VM mode  : identity tables built, MMU not enabled\n");
    }
    kprintf("VM table : %s root=0x%x count=%d\n",
            vm_table_state(),
            (unsigned int)vm_root_table(),
            (int)vm_table_count());
    kprintf("VM check : %s blocks=%d pages=%d guards=%d errors=%d\n",
            vm_validation_state(),
            (int)vm_validated_blocks(),
            (int)vm_pages_validated,
            (int)vm_guard_pages_validated,
            (int)vm_validation_errors());
    kprintf("VM map   : blocks=%d pages=%d guards=%d bytes=%d\n",
            (int)vm_mapped_blocks(),
            (int)vm_pages_mapped,
            (int)vm_guard_pages,
            (int)vm_mapped_bytes());
    kprintf("VM perms : exec_blocks=%d exec_pages=%d xn_blocks=%d xn_pages=%d\n",
            (int)vm_executable_blocks(),
            (int)vm_exec_pages,
            (int)vm_execute_never_blocks(),
            (int)vm_xn_pages);
    kprintf("VM gran  : 4 KiB kernel pages, 2 MiB blocks elsewhere\n");
    kprintf("VM attrs : normal index=%d device index=%d\n",
            (int)ARM64_ATTR_INDEX_NORMAL,
            (int)ARM64_ATTR_INDEX_DEVICE);
    kprintf("VM regs  : mair=0x%x tcr=0x%x ttbr0=0x%x\n",
            (unsigned int)arm64_mmu_read_mair(),
            (unsigned int)arm64_mmu_read_tcr(),
            (unsigned int)arm64_mmu_read_ttbr0());
    kprintf("VM sctlr : 0x%x\n", (unsigned int)arm64_mmu_read_sctlr());
    vm_dump_security_summary();
    vm_dump_named_ranges();
    vm_dump_kernel_sections();
    vm_dump_permission_goals();
    kprintf("VM regions: %d\n", (int)vm_region_count());

    for (unsigned long i = 0; i < vm_region_count(); i++)
    {
        vm_dump_region(&vm_regions[i]);
    }
}
