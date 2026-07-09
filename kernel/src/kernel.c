#include "console.h"
#include "kprintf.h"

void kernel_main(void)
{
    console_init();

    kprintf("================================\n");
    kprintf("Welcome to %s\n", "SaturnOS");
    kprintf("Version %s\n", "0.2");
    kprintf("================================\n");

    while (1)
    {
    }
}