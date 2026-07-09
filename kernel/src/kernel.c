#include "console.h"
#include "kprintf.h"

void kernel_main(void)
{
    console_init();

    kprintf("================================\n");
    kprintf("Welcome to %s\n", "SaturnOS");
    kprintf("Version %s\n", "0.2");
    kprintf("Drive: %c\n", 'A');
    kprintf("CPU: %d\n", 0);
    kprintf("Memory: %d MB\n", 512);
    kprintf("Temperature: %d C\n", -5);
    kprintf("================================\n");

    while (1)
    {
    }
}