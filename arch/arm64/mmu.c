#include "mmu.h"

int arm64_mmu_is_enabled(void)
{
    return (arm64_mmu_read_sctlr() & ARM64_SCTLR_M) != 0;
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

unsigned long arm64_mmu_table_desc(unsigned long physical)
{
    return (physical & ~(ARM64_PAGE_SIZE - 1)) |
           ARM64_DESC_VALID |
           ARM64_DESC_TABLE;
}

int arm64_mmu_desc_is_valid(unsigned long descriptor)
{
    return (descriptor & ARM64_DESC_VALID) != 0;
}

int arm64_mmu_desc_is_table(unsigned long descriptor)
{
    return (descriptor & (ARM64_DESC_VALID | ARM64_DESC_TABLE)) ==
           (ARM64_DESC_VALID | ARM64_DESC_TABLE);
}

int arm64_mmu_desc_is_l2_block(unsigned long descriptor)
{
    return (descriptor & (ARM64_DESC_VALID | ARM64_DESC_TABLE)) ==
           ARM64_DESC_VALID;
}

int arm64_mmu_desc_is_l3_page(unsigned long descriptor)
{
    return (descriptor & (ARM64_DESC_VALID | ARM64_DESC_TABLE)) ==
           (ARM64_DESC_VALID | ARM64_DESC_TABLE);
}

unsigned long arm64_mmu_desc_address(unsigned long descriptor)
{
    return descriptor & ARM64_DESC_ADDR_MASK;
}

unsigned long arm64_mmu_l2_block_address(unsigned long descriptor)
{
    return descriptor & ARM64_DESC_L2_BLOCK_ADDR_MASK;
}

unsigned long arm64_mmu_l3_page_address(unsigned long descriptor)
{
    return descriptor & ARM64_DESC_L3_PAGE_ADDR_MASK;
}

int arm64_mmu_desc_is_execute_never(unsigned long descriptor)
{
    return (descriptor & (ARM64_DESC_PXN | ARM64_DESC_UXN)) != 0;
}

const char *arm64_mmu_desc_execute_state(unsigned long descriptor)
{
    if (arm64_mmu_desc_is_execute_never(descriptor))
    {
        return "xn";
    }

    return "exec";
}

int arm64_mmu_desc_is_read_only(unsigned long descriptor)
{
    return (descriptor & ARM64_DESC_AP_MASK) == ARM64_DESC_AP_RO_EL1;
}

const char *arm64_mmu_desc_write_state(unsigned long descriptor)
{
    if (arm64_mmu_desc_is_read_only(descriptor))
    {
        return "ro";
    }

    return "rw";
}

int arm64_mmu_desc_has_user_access(unsigned long descriptor)
{
    unsigned long access = descriptor & ARM64_DESC_AP_MASK;

    return access == ARM64_DESC_AP_RW_EL0 ||
           access == ARM64_DESC_AP_RO_EL0;
}

const char *arm64_mmu_desc_user_access_state(unsigned long descriptor)
{
    if (arm64_mmu_desc_has_user_access(descriptor))
    {
        return "el0";
    }

    return "el1";
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

unsigned long arm64_mmu_read_sctlr(void)
{
    unsigned long value;

    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(value));

    return value;
}

unsigned long arm64_mmu_read_mair(void)
{
    unsigned long value;

    __asm__ volatile("mrs %0, mair_el1" : "=r"(value));

    return value;
}

unsigned long arm64_mmu_read_tcr(void)
{
    unsigned long value;

    __asm__ volatile("mrs %0, tcr_el1" : "=r"(value));

    return value;
}

unsigned long arm64_mmu_read_ttbr0(void)
{
    unsigned long value;

    __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(value));

    return value;
}

static void arm64_mmu_invalidate_tlb(void)
{
    __asm__ volatile(
        "dsb sy\n"
        "tlbi vmalle1\n"
        "dsb sy\n"
        "isb\n"
        ::: "memory");
}

void arm64_mmu_enable(unsigned long root_table)
{
    unsigned long sctlr;

    if (arm64_mmu_is_enabled())
    {
        return;
    }

    __asm__ volatile("msr mair_el1, %0" : : "r"(ARM64_MAIR_EL1_VALUE));
    __asm__ volatile("msr tcr_el1, %0" : : "r"(ARM64_TCR_EL1_VALUE));
    __asm__ volatile("msr ttbr0_el1, %0" : : "r"(root_table));

    arm64_mmu_invalidate_tlb();

    sctlr = arm64_mmu_read_sctlr();
    sctlr |= ARM64_SCTLR_M;

    __asm__ volatile(
        "msr sctlr_el1, %0\n"
        "isb\n"
        :
        : "r"(sctlr)
        : "memory");
}
