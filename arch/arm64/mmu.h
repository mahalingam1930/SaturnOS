#ifndef ARM64_MMU_H
#define ARM64_MMU_H

#define ARM64_PAGE_SHIFT 12UL
#define ARM64_PAGE_SIZE (1UL << ARM64_PAGE_SHIFT)
#define ARM64_TABLE_ENTRIES 512UL
#define ARM64_L1_SHIFT 30UL
#define ARM64_L2_SHIFT 21UL
#define ARM64_L3_SHIFT 12UL
#define ARM64_L1_BLOCK_SIZE (1UL << ARM64_L1_SHIFT)
#define ARM64_L2_BLOCK_SIZE (1UL << ARM64_L2_SHIFT)

#define ARM64_DESC_VALID (1UL << 0)
#define ARM64_DESC_TABLE (1UL << 1)
#define ARM64_DESC_BLOCK ARM64_DESC_VALID
#define ARM64_DESC_PAGE (ARM64_DESC_VALID | ARM64_DESC_TABLE)
#define ARM64_DESC_ATTR_INDEX(index) ((unsigned long)(index) << 2)
#define ARM64_DESC_AP_RW_EL1 (0UL << 6)
#define ARM64_DESC_SH_INNER (3UL << 8)
#define ARM64_DESC_AF (1UL << 10)
#define ARM64_DESC_PXN (1UL << 53)
#define ARM64_DESC_UXN (1UL << 54)

#define ARM64_ATTR_INDEX_NORMAL 0UL
#define ARM64_ATTR_INDEX_DEVICE 1UL

#define ARM64_NORMAL_MEMORY_ATTR \
    (ARM64_DESC_ATTR_INDEX(ARM64_ATTR_INDEX_NORMAL) | \
     ARM64_DESC_AP_RW_EL1 | \
     ARM64_DESC_SH_INNER | \
     ARM64_DESC_AF | \
     ARM64_DESC_PXN | \
     ARM64_DESC_UXN)

#define ARM64_DEVICE_MEMORY_ATTR \
    (ARM64_DESC_ATTR_INDEX(ARM64_ATTR_INDEX_DEVICE) | \
     ARM64_DESC_AP_RW_EL1 | \
     ARM64_DESC_AF | \
     ARM64_DESC_PXN | \
     ARM64_DESC_UXN)

int arm64_mmu_is_enabled(void);
const char *arm64_mmu_state(void);
unsigned long arm64_mmu_l1_index(unsigned long address);
unsigned long arm64_mmu_l2_index(unsigned long address);
unsigned long arm64_mmu_l3_index(unsigned long address);
unsigned long arm64_mmu_l2_block_desc(unsigned long physical,
                                      unsigned long attributes);
unsigned long arm64_mmu_l3_page_desc(unsigned long physical,
                                     unsigned long attributes);

#endif
