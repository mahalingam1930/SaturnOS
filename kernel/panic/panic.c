#include "panic.h"
#include "kprintf.h"

void kernel_panic(const char *reason,
                  unsigned long esr,
                  unsigned long elr,
                  unsigned long far,
                  unsigned long spsr)
{
    kprintf("\n");
    kprintf("====================================================\n");
    kprintf("             SaturnOS KERNEL PANIC\n");
    kprintf("====================================================\n\n");

    kprintf("Reason : %s\n\n", reason);

    kprintf("ESR_EL1 : 0x%x\n", (unsigned int)esr);
    kprintf("ELR_EL1 : 0x%x\n", (unsigned int)elr);
    kprintf("FAR_EL1 : 0x%x\n", (unsigned int)far);
    kprintf("SPSR_EL1: 0x%x\n", (unsigned int)spsr);

    kprintf("\nSystem Halted.\n");

    while (1)
    {
    }
}