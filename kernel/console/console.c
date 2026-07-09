#include "console.h"
#include "uart.h"

void console_init(void)
{
    uart_init();
}

void console_write(const char *str)
{
    uart_puts(str);
}
