#include "panic.h"
#include "kprintf.h"

void kernel_panic(const struct exception_info *info,
                  unsigned long esr,
                  unsigned long elr,
                  unsigned long far,
                  unsigned long spsr)
{
    kprintf("\n");
    kprintf("====================================================\n");
    kprintf("             SaturnOS KERNEL PANIC\n");
    kprintf("====================================================\n\n");

    kprintf("Reason : %s\n", info->exception_class);
    kprintf("EC     : 0x%x\n", (unsigned int)info->ec);
    kprintf("ISS    : 0x%x\n", (unsigned int)info->iss);

    if (info->has_fault_status)
    {
        kprintf("Access : %s\n", info->access_type);
        kprintf("FSC    : 0x%x\n", (unsigned int)info->fsc);
        kprintf("Fault  : %s\n", info->fault_status);
        kprintf("Level  : %d\n", info->fault_level);
    }

    kprintf("\n");

    kprintf("ESR_EL1 : 0x%x\n", (unsigned int)esr);
    kprintf("ELR_EL1 : 0x%x\n", (unsigned int)elr);
    kprintf("FAR_EL1 : 0x%x\n", (unsigned int)far);
    kprintf("SPSR_EL1: 0x%x\n", (unsigned int)spsr);

    kprintf("\nSystem Halted.\n");

    while (1)
    {
    }
}
