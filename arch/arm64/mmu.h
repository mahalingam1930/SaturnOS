#ifndef ARM64_MMU_H
#define ARM64_MMU_H

#define ARM64_PAGE_SHIFT 12UL
#define ARM64_PAGE_SIZE (1UL << ARM64_PAGE_SHIFT)
#define ARM64_TABLE_ENTRIES 512UL
#define ARM64_TABLE_SIZE (ARM64_TABLE_ENTRIES * sizeof(unsigned long))
#define ARM64_L1_SHIFT 30UL
#define ARM64_L2_SHIFT 21UL
#define ARM64_L3_SHIFT 12UL
#define ARM64_L1_BLOCK_SIZE (1UL << ARM64_L1_SHIFT)
#define ARM64_L2_BLOCK_SIZE (1UL << ARM64_L2_SHIFT)

#define ARM64_DESC_VALID (1UL << 0)
#define ARM64_DESC_TABLE (1UL << 1)
#define ARM64_DESC_ADDR_MASK 0x0000fffffffff000UL
#define ARM64_DESC_L2_BLOCK_ADDR_MASK 0x0000ffffffe00000UL
#define ARM64_DESC_L3_PAGE_ADDR_MASK 0x0000fffffffff000UL
#define ARM64_DESC_BLOCK ARM64_DESC_VALID
#define ARM64_DESC_PAGE (ARM64_DESC_VALID | ARM64_DESC_TABLE)
#define ARM64_DESC_ATTR_INDEX(index) ((unsigned long)(index) << 2)
#define ARM64_DESC_AP_MASK (3UL << 6)
#define ARM64_DESC_AP_RW_EL1 (0UL << 6)
#define ARM64_DESC_AP_RW_EL0 (1UL << 6)
#define ARM64_DESC_AP_RO_EL1 (2UL << 6)
#define ARM64_DESC_AP_RO_EL0 (3UL << 6)
#define ARM64_DESC_SH_INNER (3UL << 8)
#define ARM64_DESC_AF (1UL << 10)
#define ARM64_DESC_PXN (1UL << 53)
#define ARM64_DESC_UXN (1UL << 54)

#define ARM64_ATTR_INDEX_NORMAL 0UL
#define ARM64_ATTR_INDEX_DEVICE 1UL
#define ARM64_MAIR_NORMAL 0xffUL
#define ARM64_MAIR_DEVICE_NGNRNE 0x00UL

#define ARM64_MAIR_EL1_VALUE \
    (ARM64_MAIR_NORMAL << (ARM64_ATTR_INDEX_NORMAL * 8)) | \
    (ARM64_MAIR_DEVICE_NGNRNE << (ARM64_ATTR_INDEX_DEVICE * 8))

#define ARM64_TCR_T0SZ_32BIT 32UL
#define ARM64_TCR_IRGN0_WBWA (1UL << 8)
#define ARM64_TCR_ORGN0_WBWA (1UL << 10)
#define ARM64_TCR_SH0_INNER (3UL << 12)
#define ARM64_TCR_TG0_4K (0UL << 14)
#define ARM64_TCR_EPD1 (1UL << 23)
#define ARM64_TCR_IPS_32BIT (0UL << 32)

#define ARM64_TCR_EL1_VALUE \
    (ARM64_TCR_T0SZ_32BIT | \
     ARM64_TCR_IRGN0_WBWA | \
     ARM64_TCR_ORGN0_WBWA | \
     ARM64_TCR_SH0_INNER | \
     ARM64_TCR_TG0_4K | \
     ARM64_TCR_EPD1 | \
     ARM64_TCR_IPS_32BIT)

#define ARM64_SCTLR_M (1UL << 0)

#define ARM64_NORMAL_MEMORY_ATTR \
    (ARM64_DESC_ATTR_INDEX(ARM64_ATTR_INDEX_NORMAL) | \
     ARM64_DESC_AP_RW_EL1 | \
     ARM64_DESC_SH_INNER | \
     ARM64_DESC_AF)

#define ARM64_DEVICE_MEMORY_ATTR \
    (ARM64_DESC_ATTR_INDEX(ARM64_ATTR_INDEX_DEVICE) | \
     ARM64_DESC_AP_RW_EL1 | \
     ARM64_DESC_AF | \
     ARM64_DESC_PXN | \
     ARM64_DESC_UXN)

int arm64_mmu_is_enabled(void);
const char *arm64_mmu_state(void);
void arm64_mmu_enable(unsigned long root_table);
unsigned long arm64_mmu_read_sctlr(void);
unsigned long arm64_mmu_read_mair(void);
unsigned long arm64_mmu_read_tcr(void);
unsigned long arm64_mmu_read_ttbr0(void);
unsigned long arm64_mmu_l1_index(unsigned long address);
unsigned long arm64_mmu_l2_index(unsigned long address);
unsigned long arm64_mmu_l3_index(unsigned long address);
unsigned long arm64_mmu_table_desc(unsigned long physical);
int arm64_mmu_desc_is_valid(unsigned long descriptor);
int arm64_mmu_desc_is_table(unsigned long descriptor);
int arm64_mmu_desc_is_l2_block(unsigned long descriptor);
int arm64_mmu_desc_is_l3_page(unsigned long descriptor);
unsigned long arm64_mmu_desc_address(unsigned long descriptor);
unsigned long arm64_mmu_l2_block_address(unsigned long descriptor);
unsigned long arm64_mmu_l3_page_address(unsigned long descriptor);
int arm64_mmu_desc_is_execute_never(unsigned long descriptor);
const char *arm64_mmu_desc_execute_state(unsigned long descriptor);
int arm64_mmu_desc_is_read_only(unsigned long descriptor);
const char *arm64_mmu_desc_write_state(unsigned long descriptor);
unsigned long arm64_mmu_l2_block_desc(unsigned long physical,
                                      unsigned long attributes);
unsigned long arm64_mmu_l3_page_desc(unsigned long physical,
                                     unsigned long attributes);

#endif
