#include "uart.h"

#define UART_BASE 0x09000000

#define UART_DR   (*(volatile unsigned int *)(UART_BASE + 0x00))
#define UART_FR   (*(volatile unsigned int *)(UART_BASE + 0x18))

#define UART_FR_TXFF (1 << 5)

void uart_init(void)
{
    // QEMU virt PL011 UART needs no special initialization
}

void uart_putc(char c)
{
    while (UART_FR & UART_FR_TXFF)
    {
        ;
    }

    UART_DR = c;
}

void uart_puts(const char *str)
{
    while (*str)
    {
        uart_putc(*str++);
    }
}