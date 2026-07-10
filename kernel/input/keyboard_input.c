#include "keyboard_input.h"
#include "keyboard.h"
#include "scheduler.h"
#include "shell.h"

static void keyboard_input_task(void)
{
    char c;

    shell_init();

    while (1)
    {
        if (keyboard_read_char(&c))
        {
            shell_handle_char(c);
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
