#include "exception.h"
#include "kprintf.h"

void exception_init(void)
{
    __asm__ volatile (
        "adr x0, exception_vector\n"
        "msr VBAR_EL1, x0\n"
        "isb\n"
        :
        :
        : "x0"
    );
}

void exception_handler(void)
{
    kprintf("\n");
    kprintf("================================\n");
    kprintf(" SaturnOS Exception Handler\n");
    kprintf("================================\n");
    kprintf("Unhandled CPU Exception\n");

    while (1)
    {
    }
}
void exception_test(void)
{
    __asm__ volatile("brk #0");
}