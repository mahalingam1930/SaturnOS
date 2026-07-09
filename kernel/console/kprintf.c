#include "kprintf.h"
#include "console.h"

#include <stdarg.h>

static void print_decimal(int value)
{
    char buffer[12];
    int i = 0;

    if (value == 0)
    {
        console_write("0");
        return;
    }

    if (value < 0)
    {
        console_write("-");
        value = -value;
    }

    while (value > 0)
    {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0)
    {
        char c[2] = { buffer[--i], '\0' };
        console_write(c);
    }
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
                char ch = (char)va_arg(args, int);

                char str[2] = { ch, '\0' };
                console_write(str);
            }
            else if (*fmt == 'd')
            {
                int value = va_arg(args, int);
                print_decimal(value);
            }
            else
            {
                console_write("%");

                char c[2] = { *fmt, '\0' };
                console_write(c);
            }
        }
        else
        {
            char c[2] = { *fmt, '\0' };
            console_write(c);
        }

        fmt++;
    }

    va_end(args);
}