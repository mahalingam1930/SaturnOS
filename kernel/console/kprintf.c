#include "kprintf.h"
#include "console.h"

#include <stdarg.h>

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
            else
            {
                console_write("%");
                char c[2] = {*fmt, '\0'};
                console_write(c);
            }
        }
        else
        {
            char c[2] = {*fmt, '\0'};
            console_write(c);
        }

        fmt++;
    }

    va_end(args);
}