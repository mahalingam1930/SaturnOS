#include "console.h"
#include "uart.h"
#include "framebuffer.h"

void console_init(void)
{
    uart_init();
}

void console_putc(char c)
{
    uart_putc(c);
    framebuffer_console_putc(c);
}

void console_write(const char *str)
{
    while (*str)
    {
        console_putc(*str);
        str++;
    }
}
