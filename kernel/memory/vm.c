#include "vm.h"
#include "kprintf.h"
#include "mmu.h"

#define VM_L2_TABLE_CAPACITY 4UL
#define VM_UART_BASE 0x09000000UL
#define VM_UART_END 0x09001000UL
#define VM_GIC_BASE 0x08000000UL
#define VM_GIC_END 0x08020000UL
#define VM_FW_CFG_BASE 0x09020000UL
#define VM_FW_CFG_END 0x09021000UL
#define VM_RAMFB_CONTROL_BASE 0x46ffe000UL
#define VM_FRAMEBUFFER_BASE 0x47000000UL
#define VM_FRAMEBUFFER_END 0x4712c000UL

extern char _kernel_start[];
extern char _kernel_end[];

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
        ARM64_NORMAL_MEMORY_ATTR,
        "normal",
    },
};

static const struct vm_named_region vm_named_regions[] = {
    {"gic", VM_GIC_BASE, VM_GIC_END},
    {"uart", VM_UART_BASE, VM_UART_END},
    {"fw_cfg", VM_FW_CFG_BASE, VM_FW_CFG_END},
    {"kernel", (unsigned long)_kernel_start, (unsigned long)_kernel_end},
    {"ramfb-control", VM_RAMFB_CONTROL_BASE, VM_FRAMEBUFFER_BASE},
    {"framebuffer", VM_FRAMEBUFFER_BASE, VM_FRAMEBUFFER_END},
};

static unsigned long vm_l1_table[ARM64_TABLE_ENTRIES]
    __attribute__((aligned(ARM64_TABLE_SIZE)));
static unsigned long vm_l2_tables[VM_L2_TABLE_CAPACITY][ARM64_TABLE_ENTRIES]
    __attribute__((aligned(ARM64_TABLE_SIZE)));
static struct vm_l2_table_slot vm_l2_slots[VM_L2_TABLE_CAPACITY];
static int vm_ready;
static int vm_tables_built;
static int vm_validated;
static int vm_enable_attempted;
static unsigned long vm_used_l2_tables;
static unsigned long vm_blocks_mapped;
static unsigned long vm_blocks_validated;
static unsigned long vm_validation_error_count;
static unsigned long vm_exec_blocks;
static unsigned long vm_xn_blocks;
static unsigned long vm_bytes_mapped;

static void clear_table(unsigned long *table)
{
    for (unsigned long i = 0; i < ARM64_TABLE_ENTRIES; i++)
    {
        table[i] = 0;
    }
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

static int build_tables(void)
{
    clear_table(vm_l1_table);

    for (unsigned long i = 0; i < VM_L2_TABLE_CAPACITY; i++)
    {
        vm_l2_slots[i].l1_index = 0;
        vm_l2_slots[i].used = 0;
        clear_table(vm_l2_tables[i]);
    }

    vm_used_l2_tables = 0;
    vm_blocks_mapped = 0;
    vm_blocks_validated = 0;
    vm_validation_error_count = 0;
    vm_exec_blocks = 0;
    vm_xn_blocks = 0;
    vm_bytes_mapped = 0;

    for (unsigned long i = 0; i < vm_region_count(); i++)
    {
        if (!map_region(&vm_regions[i]))
        {
            return 0;
        }
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
        unsigned long l1_index = arm64_mmu_l1_index(va);
        unsigned long l2_index = arm64_mmu_l2_index(va);
        unsigned long l1_descriptor = vm_l1_table[l1_index];
        unsigned long *l2_table;
        unsigned long l2_descriptor;

        if (!arm64_mmu_desc_is_table(l1_descriptor))
        {
            validation_error();
            return 0;
        }

        l2_table = find_l2_table(l1_index);
        if (!l2_table ||
            arm64_mmu_desc_address(l1_descriptor) != (unsigned long)l2_table)
        {
            validation_error();
            return 0;
        }

        l2_descriptor = l2_table[l2_index];
        if (!arm64_mmu_desc_is_l2_block(l2_descriptor))
        {
            validation_error();
            return 0;
        }

        if (arm64_mmu_l2_block_address(l2_descriptor) != pa)
        {
            validation_error();
            return 0;
        }

        vm_blocks_validated++;
        va += ARM64_L2_BLOCK_SIZE;
        pa += ARM64_L2_BLOCK_SIZE;
        remaining -= ARM64_L2_BLOCK_SIZE;
    }

    return 1;
}

static int validate_tables(void)
{
    vm_blocks_validated = 0;
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

    return 1 + vm_used_l2_tables;
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
    unsigned long l1 = arm64_mmu_l1_index(virtual_address);
    unsigned long l2 = arm64_mmu_l2_index(virtual_address);
    unsigned long offset = virtual_address & (ARM64_L2_BLOCK_SIZE - 1);
    unsigned long l1_descriptor = vm_l1_table[l1];
    unsigned long *l2_table;
    unsigned long l2_descriptor;

    if (!arm64_mmu_desc_is_table(l1_descriptor))
    {
        return 0;
    }

    l2_table = find_l2_table(l1);
    if (!l2_table)
    {
        return 0;
    }

    l2_descriptor = l2_table[l2];
    if (!arm64_mmu_desc_is_l2_block(l2_descriptor))
    {
        return 0;
    }

    *physical = arm64_mmu_l2_block_address(l2_descriptor) + offset;
    return 1;
}

void vm_dump_walk_address(const char *label, unsigned long address)
{
    unsigned long l1 = arm64_mmu_l1_index(address);
    unsigned long l2 = arm64_mmu_l2_index(address);
    unsigned long offset = address & (ARM64_L2_BLOCK_SIZE - 1);
    unsigned long physical = 0;
    unsigned long l1_descriptor = vm_l1_table[l1];
    const char *region = vm_region_name_for_address(address);

    kprintf("VM walk %s: va=0x%x region=%s l1=%d l2=%d off=0x%x\n",
            label,
            (unsigned int)address,
            region,
            (int)l1,
            (int)l2,
            (unsigned int)offset);
    kprintf("  l1 desc=0x%x %s\n",
            (unsigned int)l1_descriptor,
            arm64_mmu_desc_is_table(l1_descriptor) ? "table" : "invalid");

    if (vm_walk_address(address, &physical))
    {
        unsigned long *l2_table = find_l2_table(l1);
        unsigned long l2_descriptor = l2_table[l2];

        kprintf("  l2 desc=0x%x block perm=%s\n",
                (unsigned int)l2_descriptor,
                arm64_mmu_desc_execute_state(l2_descriptor));
        kprintf("  pa=0x%x\n", (unsigned int)physical);
    }
    else
    {
        kprintf("  not mapped\n");
    }
}

void vm_dump_walk_examples(void)
{
    vm_dump_walk_address("uart", 0x09000000UL);
    vm_dump_walk_address("kernel", 0x40080000UL);
    vm_dump_walk_address("gap", 0x20000000UL);
}

static void vm_dump_region(const struct vm_region *region)
{
    unsigned long end = region->virtual_start + region->size;
    unsigned long l1 = arm64_mmu_l1_index(region->virtual_start);
    unsigned long l2 = arm64_mmu_l2_index(region->virtual_start);
    unsigned long descriptor =
        arm64_mmu_l2_block_desc(region->physical_start, region->attributes);

    kprintf("  %s: va=0x%x-0x%x pa=0x%x type=%s perm=%s l1=%d l2=%d\n",
            region->name,
            (unsigned int)region->virtual_start,
            (unsigned int)end,
            (unsigned int)region->physical_start,
            region->type,
            arm64_mmu_desc_execute_state(descriptor),
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
    kprintf("VM check : %s blocks=%d errors=%d\n",
            vm_validation_state(),
            (int)vm_validated_blocks(),
            (int)vm_validation_errors());
    kprintf("VM map   : blocks=%d bytes=%d\n",
            (int)vm_mapped_blocks(),
            (int)vm_mapped_bytes());
    kprintf("VM perms : exec=%d xn=%d\n",
            (int)vm_executable_blocks(),
            (int)vm_execute_never_blocks());
    kprintf("VM gran  : 4 KiB pages, 2 MiB L2 blocks\n");
    kprintf("VM attrs : normal index=%d device index=%d\n",
            (int)ARM64_ATTR_INDEX_NORMAL,
            (int)ARM64_ATTR_INDEX_DEVICE);
    kprintf("VM regs  : mair=0x%x tcr=0x%x ttbr0=0x%x\n",
            (unsigned int)arm64_mmu_read_mair(),
            (unsigned int)arm64_mmu_read_tcr(),
            (unsigned int)arm64_mmu_read_ttbr0());
    kprintf("VM sctlr : 0x%x\n", (unsigned int)arm64_mmu_read_sctlr());
    kprintf("VM regions: %d\n", (int)vm_region_count());

    for (unsigned long i = 0; i < vm_region_count(); i++)
    {
        vm_dump_region(&vm_regions[i]);
    }
}
