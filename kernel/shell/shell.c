#include "shell.h"
#include "exception.h"
#include "framebuffer.h"
#include "heap.h"
#include "keyboard.h"
#include "kprintf.h"
#include "pmm.h"
#include "scheduler.h"
#include "timer.h"
#include "version.h"
#include "vm.h"

#define SHELL_BUFFER_SIZE 64
#define SHELL_HISTORY_SIZE 8
#define SHELL_PROMPT "saturn> "

static char command_buffer[SHELL_BUFFER_SIZE];
static char history_buffer[SHELL_HISTORY_SIZE][SHELL_BUFFER_SIZE];
static unsigned int command_length;
static unsigned int command_cursor;
static unsigned int rendered_length;
static unsigned int escape_state;
static unsigned int history_count;
static unsigned int history_next;
static unsigned int history_view;

static void shell_home(void);
static void shell_end(void);

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
    kprintf(SHELL_PROMPT);
    rendered_length = 0;
}

static void copy_string(char *destination, const char *source)
{
    unsigned int i = 0;

    while (source[i] && i < SHELL_BUFFER_SIZE - 1)
    {
        destination[i] = source[i];
        i++;
    }

    destination[i] = '\0';
}

static unsigned int string_length(const char *text)
{
    unsigned int length = 0;

    while (text[length])
    {
        length++;
    }

    return length;
}

static unsigned int history_index(unsigned int logical_index)
{
    unsigned int first;

    if (history_count < SHELL_HISTORY_SIZE)
    {
        return logical_index;
    }

    first = history_next;
    return (first + logical_index) % SHELL_HISTORY_SIZE;
}

static const char *history_entry(unsigned int logical_index)
{
    return history_buffer[history_index(logical_index)];
}

static void history_add(const char *command)
{
    if (command[0] == '\0')
    {
        return;
    }

    if (history_count > 0 &&
        string_equals(command, history_entry(history_count - 1)))
    {
        history_view = history_count;
        return;
    }

    copy_string(history_buffer[history_next], command);
    history_next = (history_next + 1) % SHELL_HISTORY_SIZE;

    if (history_count < SHELL_HISTORY_SIZE)
    {
        history_count++;
    }

    history_view = history_count;
}

static void shell_move_left(unsigned int count)
{
    while (count > 0)
    {
        kprintf("\b");
        count--;
    }
}

static void shell_move_right(unsigned int count)
{
    while (count > 0 && command_cursor < command_length)
    {
        kprintf("%c", command_buffer[command_cursor]);
        command_cursor++;
        count--;
    }
}

static void shell_redraw_from_cursor(unsigned int old_length)
{
    unsigned int printed = 0;
    unsigned int erase_count;

    for (unsigned int i = command_cursor; i < command_length; i++)
    {
        kprintf("%c", command_buffer[i]);
        printed++;
    }

    if (old_length > command_length)
    {
        erase_count = old_length - command_length;
        for (unsigned int i = 0; i < erase_count; i++)
        {
            kprintf(" ");
        }
        printed += erase_count;
    }

    shell_move_left(printed);
    rendered_length = command_length;
}

static void shell_replace_line(const char *text)
{
    unsigned int old_length = command_length;

    shell_home();
    copy_string(command_buffer, text);
    command_length = string_length(command_buffer);
    command_cursor = 0;
    shell_redraw_from_cursor(old_length);
    shell_end();
}

static void shell_insert_char(char c)
{
    if (command_length >= SHELL_BUFFER_SIZE - 1)
    {
        kprintf("\nCommand too long\n");
        command_length = 0;
        command_cursor = 0;
        rendered_length = 0;
        shell_prompt();
        return;
    }

    for (unsigned int i = command_length; i > command_cursor; i--)
    {
        command_buffer[i] = command_buffer[i - 1];
    }

    command_buffer[command_cursor] = c;
    command_length++;
    kprintf("%c", c);
    command_cursor++;

    if (command_cursor < command_length)
    {
        unsigned int saved_cursor = command_cursor;

        for (unsigned int i = command_cursor; i < command_length; i++)
        {
            kprintf("%c", command_buffer[i]);
        }

        shell_move_left(command_length - saved_cursor);
    }

    rendered_length = command_length;
}

static void shell_backspace(void)
{
    unsigned int old_length = command_length;

    if (command_cursor == 0)
    {
        return;
    }

    command_cursor--;
    kprintf("\b");

    for (unsigned int i = command_cursor; i < command_length - 1; i++)
    {
        command_buffer[i] = command_buffer[i + 1];
    }

    command_length--;
    shell_redraw_from_cursor(old_length);
}

static void shell_delete(void)
{
    unsigned int old_length = command_length;

    if (command_cursor >= command_length)
    {
        return;
    }

    for (unsigned int i = command_cursor; i < command_length - 1; i++)
    {
        command_buffer[i] = command_buffer[i + 1];
    }

    command_length--;
    shell_redraw_from_cursor(old_length);
}

static void shell_home(void)
{
    shell_move_left(command_cursor);
    command_cursor = 0;
}

static void shell_end(void)
{
    shell_move_right(command_length - command_cursor);
}

static void shell_history_previous(void)
{
    if (history_count == 0)
    {
        return;
    }

    if (history_view == 0)
    {
        return;
    }

    history_view--;
    shell_replace_line(history_entry(history_view));
}

static void shell_history_next(void)
{
    if (history_count == 0 || history_view >= history_count)
    {
        return;
    }

    history_view++;

    if (history_view == history_count)
    {
        shell_replace_line("");
        return;
    }

    shell_replace_line(history_entry(history_view));
}

static int shell_handle_escape_char(char c)
{
    if (escape_state == 0)
    {
        return 0;
    }

    if (escape_state == 1)
    {
        escape_state = (c == '[') ? 2 : 0;
        return 1;
    }

    if (escape_state == 2)
    {
        escape_state = 0;

        if (c == 'D')
        {
            if (command_cursor > 0)
            {
                command_cursor--;
                kprintf("\b");
            }
            return 1;
        }

        if (c == 'A')
        {
            shell_history_previous();
            return 1;
        }

        if (c == 'B')
        {
            shell_history_next();
            return 1;
        }

        if (c == 'C')
        {
            shell_move_right(1);
            return 1;
        }

        if (c == 'H')
        {
            shell_home();
            return 1;
        }

        if (c == 'F')
        {
            shell_end();
            return 1;
        }

        if (c == '3')
        {
            escape_state = 3;
            return 1;
        }

        return 1;
    }

    escape_state = 0;
    if (c == '~')
    {
        shell_delete();
    }

    return 1;
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
    command_cursor = 0;
    rendered_length = 0;
    escape_state = 0;
    history_view = history_count;
    kprintf("SaturnOS shell ready\n");
    shell_prompt();
}

void shell_handle_char(char c)
{
    if (c == '\r')
    {
        c = '\n';
    }

    if (shell_handle_escape_char(c))
    {
        return;
    }

    if (c == 0x1b)
    {
        escape_state = 1;
        return;
    }

    if (c == KEYBOARD_CHAR_HOME)
    {
        shell_home();
        return;
    }

    if (c == KEYBOARD_CHAR_LEFT)
    {
        if (command_cursor > 0)
        {
            command_cursor--;
            kprintf("\b");
        }
        return;
    }

    if (c == KEYBOARD_CHAR_UP)
    {
        shell_history_previous();
        return;
    }

    if (c == KEYBOARD_CHAR_DELETE)
    {
        shell_delete();
        return;
    }

    if (c == KEYBOARD_CHAR_END)
    {
        shell_end();
        return;
    }

    if (c == KEYBOARD_CHAR_DOWN)
    {
        shell_history_next();
        return;
    }

    if (c == KEYBOARD_CHAR_RIGHT)
    {
        shell_move_right(1);
        return;
    }

    if (c == '\n')
    {
        command_buffer[command_length] = '\0';
        kprintf("\n");
        history_add(command_buffer);
        shell_execute(command_buffer);
        command_length = 0;
        command_cursor = 0;
        rendered_length = 0;
        escape_state = 0;
        history_view = history_count;
        shell_prompt();
        return;
    }

    if (c == '\b' || c == 0x7f)
    {
        shell_backspace();
        return;
    }

    if (c < 32 || c > 126)
    {
        return;
    }

    history_view = history_count;
    shell_insert_char(c);
}
