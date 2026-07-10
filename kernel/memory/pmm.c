#include "pmm.h"
#include "kprintf.h"

#define PMM_TOTAL_PAGES (PMM_RAM_SIZE / PMM_PAGE_SIZE)
#define RAMFB_RESERVED_BASE 0x46ffe000UL
#define RAMFB_RESERVED_END 0x47200000UL

extern char _kernel_start[];
extern char _kernel_end[];

static unsigned char page_bitmap[PMM_TOTAL_PAGES / 8];
static unsigned long used_pages;

static unsigned long align_down(unsigned long value)
{
    return value & ~(PMM_PAGE_SIZE - 1);
}

static unsigned long align_up(unsigned long value)
{
    return (value + PMM_PAGE_SIZE - 1) & ~(PMM_PAGE_SIZE - 1);
}

static unsigned long page_index(unsigned long address)
{
    return (address - PMM_RAM_BASE) / PMM_PAGE_SIZE;
}

static int page_is_used(unsigned long index)
{
    return (page_bitmap[index / 8] & (1U << (index % 8))) != 0;
}

static void mark_page_used(unsigned long index)
{
    if (!page_is_used(index))
    {
        page_bitmap[index / 8] |= (unsigned char)(1U << (index % 8));
        used_pages++;
    }
}

static void mark_page_free(unsigned long index)
{
    if (page_is_used(index))
    {
        page_bitmap[index / 8] &= (unsigned char)~(1U << (index % 8));
        used_pages--;
    }
}

static void reserve_range(unsigned long start, unsigned long end)
{
    unsigned long current;

    if (end <= PMM_RAM_BASE || start >= PMM_RAM_END)
    {
        return;
    }

    if (start < PMM_RAM_BASE)
    {
        start = PMM_RAM_BASE;
    }

    if (end > PMM_RAM_END)
    {
        end = PMM_RAM_END;
    }

    start = align_down(start);
    end = align_up(end);

    for (current = start; current < end; current += PMM_PAGE_SIZE)
    {
        mark_page_used(page_index(current));
    }
}

void pmm_init(void)
{
    for (unsigned long i = 0; i < sizeof(page_bitmap); i++)
    {
        page_bitmap[i] = 0;
    }

    used_pages = 0;

    reserve_range(PMM_RAM_BASE, (unsigned long)_kernel_end);
    reserve_range(RAMFB_RESERVED_BASE, RAMFB_RESERVED_END);
}

void *pmm_alloc_page(void)
{
    for (unsigned long i = 0; i < PMM_TOTAL_PAGES; i++)
    {
        if (!page_is_used(i))
        {
            mark_page_used(i);
            return (void *)(PMM_RAM_BASE + (i * PMM_PAGE_SIZE));
        }
    }

    return 0;
}

void pmm_free_page(void *page)
{
    unsigned long address = (unsigned long)page;

    if (address < PMM_RAM_BASE || address >= PMM_RAM_END)
    {
        return;
    }

    if ((address & (PMM_PAGE_SIZE - 1)) != 0)
    {
        return;
    }

    mark_page_free(page_index(address));
}

unsigned long pmm_total_pages(void)
{
    return PMM_TOTAL_PAGES;
}

unsigned long pmm_used_pages(void)
{
    return used_pages;
}

unsigned long pmm_free_pages(void)
{
    return PMM_TOTAL_PAGES - used_pages;
}

void pmm_dump_stats(void)
{
    kprintf("PMM RAM  : 0x%x - 0x%x\n",
            (unsigned int)PMM_RAM_BASE,
            (unsigned int)PMM_RAM_END);
    kprintf("PMM page : %d bytes\n", (int)PMM_PAGE_SIZE);
    kprintf("PMM pages: total=%d used=%d free=%d\n",
            (int)pmm_total_pages(),
            (int)pmm_used_pages(),
            (int)pmm_free_pages());
    kprintf("PMM kern : 0x%x - 0x%x\n",
            (unsigned int)(unsigned long)_kernel_start,
            (unsigned int)(unsigned long)_kernel_end);
    kprintf("PMM ramfb: 0x%x - 0x%x\n",
            (unsigned int)RAMFB_RESERVED_BASE,
            (unsigned int)RAMFB_RESERVED_END);
}
