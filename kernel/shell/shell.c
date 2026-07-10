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

static const char *skip_spaces(const char *text)
{
    while (*text == ' ')
    {
        text++;
    }

    return text;
}

static int command_equals(const char *command, const char *name)
{
    while (*name)
    {
        if (*command != *name)
        {
            return 0;
        }

        command++;
        name++;
    }

    return *command == '\0' || *command == ' ';
}

static const char *command_arg(const char *command)
{
    while (*command && *command != ' ')
    {
        command++;
    }

    return skip_spaces(command);
}

static int hex_digit_value(char c, unsigned long *value)
{
    if (c >= '0' && c <= '9')
    {
        *value = (unsigned long)(c - '0');
        return 1;
    }

    if (c >= 'a' && c <= 'f')
    {
        *value = (unsigned long)(c - 'a' + 10);
        return 1;
    }

    if (c >= 'A' && c <= 'F')
    {
        *value = (unsigned long)(c - 'A' + 10);
        return 1;
    }

    return 0;
}

static int parse_number(const char *text, unsigned long *value)
{
    unsigned long result = 0;
    unsigned long base = 10;
    unsigned long digit;
    int found_digit = 0;

    text = skip_spaces(text);

    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
        base = 16;
        text += 2;
    }

    while (*text)
    {
        if (base == 16)
        {
            if (!hex_digit_value(*text, &digit))
            {
                return 0;
            }
        }
        else
        {
            if (*text < '0' || *text > '9')
            {
                return 0;
            }

            digit = (unsigned long)(*text - '0');
        }

        result = (result * base) + digit;
        found_digit = 1;
        text++;
    }

    if (!found_digit)
    {
        return 0;
    }

    *value = result;
    return 1;
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
    kprintf("  vmwalk   walk sample or given virtual address\n");
    kprintf("  ticks    show scheduler/timer ticks\n");
    kprintf("  clear    clear framebuffer console\n");
    kprintf("  panic    trigger test exception\n");
    kprintf("  fault    trigger test page fault\n");
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
    else if (command_equals(command, "vmwalk"))
    {
        const char *arg = command_arg(command);

        if (*arg == '\0')
        {
            vm_dump_walk_examples();
        }
        else
        {
            unsigned long address;

            if (parse_number(arg, &address))
            {
                vm_dump_walk_address("arg", address);
            }
            else
            {
                kprintf("Usage: vmwalk [address]\n");
                kprintf("Example: vmwalk 0x40080000\n");
            }
        }
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
    else if (string_equals(command, "fault"))
    {
        exception_test_page_fault();
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
