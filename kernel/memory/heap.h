#ifndef HEAP_H
#define HEAP_H

void heap_init(void);
void *kmalloc(unsigned long size);
void *kzalloc(unsigned long size);
void kfree(void *ptr);
unsigned long heap_total_bytes(void);
unsigned long heap_used_bytes(void);
unsigned long heap_free_bytes(void);
unsigned long heap_overhead_bytes(void);
unsigned long heap_page_count(void);
void heap_dump_stats(void);
void heap_self_test(void);

#endif
