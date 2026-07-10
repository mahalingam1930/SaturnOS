#include "vm.h"
#include "kprintf.h"
#include "mmu.h"

#define VM_L2_TABLE_CAPACITY 4UL

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

static unsigned long vm_l1_table[ARM64_TABLE_ENTRIES]
    __attribute__((aligned(ARM64_TABLE_SIZE)));
static unsigned long vm_l2_tables[VM_L2_TABLE_CAPACITY][ARM64_TABLE_ENTRIES]
    __attribute__((aligned(ARM64_TABLE_SIZE)));
static struct vm_l2_table_slot vm_l2_slots[VM_L2_TABLE_CAPACITY];
static int vm_ready;
static int vm_enable_attempted;
static unsigned long vm_used_l2_tables;
static unsigned long vm_blocks_mapped;
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

void vm_init(void)
{
    vm_ready = build_tables();

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
    if (vm_ready)
    {
        return "ready";
    }

    return "error";
}

unsigned long vm_table_count(void)
{
    if (!vm_ready)
    {
        return 0;
    }

    return 1 + vm_used_l2_tables;
}

unsigned long vm_mapped_blocks(void)
{
    return vm_blocks_mapped;
}

unsigned long vm_mapped_bytes(void)
{
    return vm_bytes_mapped;
}

unsigned long vm_root_table(void)
{
    return (unsigned long)vm_l1_table;
}

static void vm_dump_region(const struct vm_region *region)
{
    unsigned long end = region->virtual_start + region->size;
    unsigned long l1 = arm64_mmu_l1_index(region->virtual_start);
    unsigned long l2 = arm64_mmu_l2_index(region->virtual_start);

    kprintf("  %s: va=0x%x-0x%x pa=0x%x type=%s l1=%d l2=%d\n",
            region->name,
            (unsigned int)region->virtual_start,
            (unsigned int)end,
            (unsigned int)region->physical_start,
            region->type,
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
    kprintf("VM map   : blocks=%d bytes=%d\n",
            (int)vm_mapped_blocks(),
            (int)vm_mapped_bytes());
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
