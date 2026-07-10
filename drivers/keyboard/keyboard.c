#include "keyboard.h"
#include "uart.h"

void keyboard_init(void)
{
}

int keyboard_read_char(char *out)
{
    if (!uart_read_ready())
    {
        return 0;
    }

    *out = uart_getc();
    return 1;
}
