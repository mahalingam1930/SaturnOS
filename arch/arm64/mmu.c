#include "mmu.h"

int arm64_mmu_is_enabled(void)
{
    unsigned long sctlr;

    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));

    return (sctlr & 1UL) != 0;
}

const char *arm64_mmu_state(void)
{
    if (arm64_mmu_is_enabled())
    {
        return "enabled";
    }

    return "disabled";
}

unsigned long arm64_mmu_l1_index(unsigned long address)
{
    return (address >> ARM64_L1_SHIFT) & (ARM64_TABLE_ENTRIES - 1);
}

unsigned long arm64_mmu_l2_index(unsigned long address)
{
    return (address >> ARM64_L2_SHIFT) & (ARM64_TABLE_ENTRIES - 1);
}

unsigned long arm64_mmu_l3_index(unsigned long address)
{
    return (address >> ARM64_L3_SHIFT) & (ARM64_TABLE_ENTRIES - 1);
}

unsigned long arm64_mmu_l2_block_desc(unsigned long physical,
                                      unsigned long attributes)
{
    return (physical & ~(ARM64_L2_BLOCK_SIZE - 1)) |
           attributes |
           ARM64_DESC_BLOCK;
}

unsigned long arm64_mmu_l3_page_desc(unsigned long physical,
                                     unsigned long attributes)
{
    return (physical & ~(ARM64_PAGE_SIZE - 1)) |
           attributes |
           ARM64_DESC_PAGE;
}
