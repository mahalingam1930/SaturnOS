#include "keyboard_input.h"
#include "keyboard.h"
#include "kprintf.h"
#include "scheduler.h"

static void keyboard_input_task(void)
{
    char c;

    kprintf("Keyboard input ready\n");

    while (1)
    {
        if (keyboard_read_char(&c))
        {
            if (c == '\r')
            {
                c = '\n';
            }

            kprintf("%c", c);
        }
        else
        {
            scheduler_yield();
        }
    }
}

void keyboard_input_init(void)
{
    keyboard_init();
    scheduler_create_kernel_thread("keyboard", keyboard_input_task);
}
