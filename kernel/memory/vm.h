#ifndef VM_H
#define VM_H

void vm_init(void);
void vm_dump_plan(void);
unsigned long vm_region_count(void);
unsigned long vm_table_count(void);
unsigned long vm_mapped_blocks(void);
unsigned long vm_mapped_bytes(void);
unsigned long vm_root_table(void);
const char *vm_state(void);
const char *vm_table_state(void);

#endif
