#include "heap.h"
#include "kprintf.h"
#include "pmm.h"

#define HEAP_ALIGN 16UL
#define HEAP_MIN_SPLIT 32UL

struct heap_block
{
    unsigned long size;
    unsigned int free;
    struct heap_block *next;
};

static struct heap_block *heap_head;
static struct heap_block *heap_tail;
static unsigned long heap_pages;
static unsigned long heap_used;

static unsigned long align_up(unsigned long value)
{
    return (value + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1);
}

static unsigned long heap_block_capacity(void)
{
    return PMM_PAGE_SIZE - sizeof(struct heap_block);
}

static void split_block(struct heap_block *block, unsigned long size)
{
    struct heap_block *next;
    unsigned long remaining;

    if (block->size < size + sizeof(struct heap_block) + HEAP_MIN_SPLIT)
    {
        return;
    }

    remaining = block->size - size - sizeof(struct heap_block);
    next = (struct heap_block *)((char *)(block + 1) + size);
    next->size = remaining;
    next->free = 1;
    next->next = block->next;

    block->size = size;
    block->next = next;

    if (heap_tail == block)
    {
        heap_tail = next;
    }
}

static void coalesce_blocks(void)
{
    struct heap_block *block = heap_head;

    while (block && block->next)
    {
        char *block_end = (char *)(block + 1) + block->size;

        if (block->free && block->next->free &&
            block_end == (char *)block->next)
        {
            block->size += sizeof(struct heap_block) + block->next->size;
            block->next = block->next->next;

            if (block->next == 0)
            {
                heap_tail = block;
            }

            continue;
        }

        block = block->next;
    }
}

static struct heap_block *find_free_block(unsigned long size)
{
    struct heap_block *block = heap_head;

    while (block)
    {
        if (block->free && block->size >= size)
        {
            return block;
        }

        block = block->next;
    }

    return 0;
}

static struct heap_block *grow_heap(void)
{
    struct heap_block *block = (struct heap_block *)pmm_alloc_page();

    if (!block)
    {
        return 0;
    }

    block->size = heap_block_capacity();
    block->free = 1;
    block->next = 0;

    if (!heap_head)
    {
        heap_head = block;
        heap_tail = block;
    }
    else
    {
        heap_tail->next = block;
        heap_tail = block;
    }

    heap_pages++;

    return block;
}

void heap_init(void)
{
    heap_head = 0;
    heap_tail = 0;
    heap_pages = 0;
    heap_used = 0;
}

void *kmalloc(unsigned long size)
{
    struct heap_block *block;

    if (size == 0)
    {
        return 0;
    }

    size = align_up(size);

    if (size > heap_block_capacity())
    {
        return 0;
    }

    block = find_free_block(size);
    if (!block)
    {
        block = grow_heap();
        if (!block)
        {
            return 0;
        }
    }

    split_block(block, size);
    block->free = 0;
    heap_used += block->size;

    return block + 1;
}

void *kzalloc(unsigned long size)
{
    unsigned char *ptr = (unsigned char *)kmalloc(size);

    if (!ptr)
    {
        return 0;
    }

    for (unsigned long i = 0; i < size; i++)
    {
        ptr[i] = 0;
    }

    return ptr;
}

void kfree(void *ptr)
{
    struct heap_block *block;

    if (!ptr)
    {
        return;
    }

    block = ((struct heap_block *)ptr) - 1;

    if (block->free)
    {
        return;
    }

    block->free = 1;
    heap_used -= block->size;
    coalesce_blocks();
}

unsigned long heap_total_bytes(void)
{
    return heap_pages * heap_block_capacity();
}

unsigned long heap_used_bytes(void)
{
    return heap_used;
}

unsigned long heap_free_bytes(void)
{
    struct heap_block *block = heap_head;
    unsigned long total = 0;

    while (block)
    {
        if (block->free)
        {
            total += block->size;
        }

        block = block->next;
    }

    return total;
}

unsigned long heap_page_count(void)
{
    return heap_pages;
}

unsigned long heap_overhead_bytes(void)
{
    unsigned long total = heap_total_bytes();
    unsigned long used = heap_used_bytes();
    unsigned long free = heap_free_bytes();

    if (total < used + free)
    {
        return 0;
    }

    return total - used - free;
}

void heap_dump_stats(void)
{
    kprintf("Heap pages: %d\n", (int)heap_page_count());
    kprintf("Heap bytes: total=%d used=%d free=%d overhead=%d\n",
            (int)heap_total_bytes(),
            (int)heap_used_bytes(),
            (int)heap_free_bytes(),
            (int)heap_overhead_bytes());
}

void heap_self_test(void)
{
    void *small;
    void *medium;

    kprintf("Heap self-test start\n");
    heap_dump_stats();

    small = kmalloc(64);
    medium = kzalloc(128);

    kprintf("kmalloc(64)  = 0x%x\n", (unsigned int)(unsigned long)small);
    kprintf("kzalloc(128) = 0x%x\n", (unsigned int)(unsigned long)medium);
    heap_dump_stats();

    kfree(small);
    kfree(medium);

    kprintf("Heap self-test freed allocations\n");
    heap_dump_stats();
}
