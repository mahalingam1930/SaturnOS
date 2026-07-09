#include "kprintf.h"
#include "console.h"

#include <stdarg.h>

static void print_hex(unsigned int value)
{
    char buffer[9];
    unsigned int i;
    char *p;

    if (value == 0)
    {
        console_putc('0');
        return;
    }

    for (i = 0; i < 8; i++)
    {
        unsigned int nibble = (value >> (28 - i * 4)) & 0xF;
        buffer[i] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
    }
    buffer[8] = '\0';

    p = buffer;
    while (*p == '0' && *(p + 1) != '\0') p++;
    console_write(p);
}

static void print_decimal(int value)
{
    char buffer[12];
    int i = 0;

    if (value == 0)
    {
        console_putc('0');
        return;
    }

    if (value < 0)
    {
        console_putc('-');
        value = -value;
    }

    while (value > 0)
    {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0)
        console_putc(buffer[--i]);
}

void kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    while (*fmt)
    {
        if (*fmt == '%')
        {
            fmt++;

            if (*fmt == 's')
            {
                const char *str = va_arg(args, const char *);

                if (str == 0)
                    str = "(null)";

                console_write(str);
            }
            else if (*fmt == 'c')
            {
                console_putc((char)va_arg(args, int));
            }
            else if (*fmt == 'd')
            {
                print_decimal(va_arg(args, int));
            }
            else if (*fmt == 'x')
            {
                print_hex(va_arg(args, unsigned int));
            }
            else
            {
                console_putc('%');
                console_putc(*fmt);
            }
        }
        else
        {
            console_putc(*fmt);
        }

        fmt++;
    }

    va_end(args);
}
