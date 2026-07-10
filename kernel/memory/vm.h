#ifndef VM_H
#define VM_H

void vm_init(void);
void vm_dump_plan(void);
unsigned long vm_region_count(void);
const char *vm_state(void);

#endif
