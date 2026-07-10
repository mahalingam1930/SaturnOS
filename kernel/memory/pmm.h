#ifndef PMM_H
#define PMM_H

#define PMM_PAGE_SIZE 4096UL
#define PMM_RAM_BASE 0x40000000UL
#define PMM_RAM_SIZE 0x08000000UL
#define PMM_RAM_END (PMM_RAM_BASE + PMM_RAM_SIZE)

void pmm_init(void);
void *pmm_alloc_page(void);
void pmm_free_page(void *page);
unsigned long pmm_total_pages(void);
unsigned long pmm_used_pages(void);
unsigned long pmm_free_pages(void);
void pmm_dump_stats(void);

#endif
