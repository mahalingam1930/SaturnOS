#ifndef VM_H
#define VM_H

void vm_init(void);
void vm_dump_plan(void);
unsigned long vm_region_count(void);
unsigned long vm_table_count(void);
unsigned long vm_mapped_blocks(void);
unsigned long vm_validated_blocks(void);
unsigned long vm_validation_errors(void);
unsigned long vm_executable_blocks(void);
unsigned long vm_execute_never_blocks(void);
unsigned long vm_mapped_bytes(void);
unsigned long vm_root_table(void);
int vm_mmu_enabled(void);
const char *vm_state(void);
const char *vm_table_state(void);
const char *vm_validation_state(void);

#endif
