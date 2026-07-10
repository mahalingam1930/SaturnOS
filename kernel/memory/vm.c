#include "vm.h"
#include "kprintf.h"
#include "mmu.h"

struct vm_region
{
    const char *name;
    unsigned long virtual_start;
    unsigned long physical_start;
    unsigned long size;
    unsigned long attributes;
    const char *type;
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

static int vm_ready;

void vm_init(void)
{
    vm_ready = 1;
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
    kprintf("VM mode  : planned identity map, MMU not enabled here\n");
    kprintf("VM table : 4 KiB granule, L1/L2/L3 descriptors\n");
    kprintf("VM attrs : normal index=%d device index=%d\n",
            (int)ARM64_ATTR_INDEX_NORMAL,
            (int)ARM64_ATTR_INDEX_DEVICE);
    kprintf("VM regions: %d\n", (int)vm_region_count());

    for (unsigned long i = 0; i < vm_region_count(); i++)
    {
        vm_dump_region(&vm_regions[i]);
    }
}
