#include "uart.h"
#include "kprintf.h"
#include "exception.h"

void kernel_main(void)
{
    uart_init();

    exception_init();

    kprintf("================================\n");
    kprintf("Welcome to %s\n", "SaturnOS");
    kprintf("Version %s\n", "0.2");
    kprintf("Drive: %c\n", 'A');
    kprintf("CPU: %d\n", 0);
    kprintf("Memory: %d MB\n", 512);
    kprintf("Temperature: %d C\n", -5);
    kprintf("UART Base: 0x%x\n", 0x09000000);
    kprintf("Magic Number: 0x%x\n", 0xDEADBEEF);
    kprintf("================================\n");
    
    kprintf("Triggering test exception...\n");

    exception_test();

    while (1)
    {
    }
}