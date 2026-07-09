#include "uart.h"

void kernel_main(void)
{
    uart_init();
    uart_puts("Welcome to SaturnOS!\n");

    while (1)
    {
    }
}
