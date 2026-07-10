#include "shell.h"
#include "exception.h"
#include "framebuffer.h"
#include "heap.h"
#include "kprintf.h"
#include "pmm.h"
#include "scheduler.h"
#include "timer.h"
#include "version.h"
#include "vm.h"

#define SHELL_BUFFER_SIZE 64

static char command_buffer[SHELL_BUFFER_SIZE];
static unsigned int command_length;

static int string_equals(const char *left, const char *right)
{
    while (*left && *right)
    {
        if (*left != *right)
        {
            return 0;
        }

        left++;
        right++;
    }

    return *left == *right;
}

static void shell_prompt(void)
{
    kprintf("saturn> ");
}

static void shell_help(void)
{
    kprintf("Commands:\n");
    kprintf("  help     show commands\n");
    kprintf("  version  show kernel version\n");
    kprintf("  tasks    show scheduler tasks\n");
    kprintf("  mem      show physical memory stats\n");
    kprintf("  heap     show kernel heap stats\n");
    kprintf("  heaptest run heap allocation test\n");
    kprintf("  vm       show virtual memory plan\n");
    kprintf("  ticks    show scheduler/timer ticks\n");
    kprintf("  clear    clear framebuffer console\n");
    kprintf("  panic    trigger test exception\n");
}

static void shell_execute(const char *command)
{
    if (command[0] == '\0')
    {
        return;
    }

    if (string_equals(command, "help"))
    {
        shell_help();
    }
    else if (string_equals(command, "version"))
    {
        kprintf("%s %s (%s)\n",
                SATURNOS_NAME,
                SATURNOS_VERSION,
                SATURNOS_CODENAME);
    }
    else if (string_equals(command, "tasks"))
    {
        scheduler_dump_tasks();
    }
    else if (string_equals(command, "mem"))
    {
        pmm_dump_stats();
    }
    else if (string_equals(command, "heap"))
    {
        heap_dump_stats();
    }
    else if (string_equals(command, "heaptest"))
    {
        heap_self_test();
    }
    else if (string_equals(command, "vm"))
    {
        vm_dump_plan();
    }
    else if (string_equals(command, "ticks"))
    {
        kprintf("scheduler ticks=%d timer irqs=%d\n",
                (int)scheduler_get_ticks(),
                (int)timer_get_irq_ticks());
    }
    else if (string_equals(command, "clear"))
    {
        framebuffer_console_init();
    }
    else if (string_equals(command, "panic"))
    {
        exception_test();
    }
    else
    {
        kprintf("Unknown command: %s\n", command);
        kprintf("Type 'help' for commands\n");
    }
}

void shell_init(void)
{
    command_length = 0;
    kprintf("SaturnOS shell ready\n");
    shell_prompt();
}

void shell_handle_char(char c)
{
    if (c == '\r')
    {
        c = '\n';
    }

    if (c == '\n')
    {
        command_buffer[command_length] = '\0';
        kprintf("\n");
        shell_execute(command_buffer);
        command_length = 0;
        shell_prompt();
        return;
    }

    if (c == '\b' || c == 0x7f)
    {
        if (command_length > 0)
        {
            command_length--;
            kprintf("\b \b");
        }
        return;
    }

    if (c < 32 || c > 126)
    {
        return;
    }

    if (command_length >= SHELL_BUFFER_SIZE - 1)
    {
        kprintf("\nCommand too long\n");
        command_length = 0;
        shell_prompt();
        return;
    }

    command_buffer[command_length++] = c;
    kprintf("%c", c);
}
