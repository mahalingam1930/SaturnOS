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

struct shell_command
{
    const char *name;
    const char *usage;
    const char *description;
    const char *example;
};

struct shell_alias
{
    const char *name;
    const char *target;
    const char *description;
};

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

static const struct shell_command shell_commands[] = {
    {"help", "help [command]", "show commands or detailed command help", "help vmwalk"},
    {"version", "version", "show kernel version", 0},
    {"tasks", "tasks", "show scheduler tasks", 0},
    {"mem", "mem", "show physical memory stats", 0},
    {"heap", "heap", "show kernel heap stats", 0},
    {"heaptest", "heaptest", "run heap allocation test", 0},
    {"vm", "vm", "show virtual memory plan", 0},
    {"vmwalk", "vmwalk [address]", "walk sample or given virtual address", "vmwalk 0x40080000"},
    {"ticks", "ticks", "show scheduler/timer ticks", 0},
    {"clear", "clear", "clear framebuffer console", 0},
    {"panic", "panic", "trigger test exception", 0},
    {"fault", "fault", "trigger test page fault", 0},
};

static const struct shell_alias shell_aliases[] = {
    {"h", "help", "show commands"},
    {"?", "help", "show commands"},
    {"ps", "tasks", "show scheduler tasks"},
    {"free", "mem", "show physical memory stats"},
    {"top", "tasks", "show scheduler tasks"},
    {"uptime", "ticks", "show scheduler/timer ticks"},
    {"cls", "clear", "clear framebuffer console"},
};

static const unsigned int shell_command_count =
    sizeof(shell_commands) / sizeof(shell_commands[0]);

static const unsigned int shell_alias_count =
    sizeof(shell_aliases) / sizeof(shell_aliases[0]);

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

static const char *command_alias_target(const char *command)
{
    for (unsigned int i = 0; i < shell_alias_count; i++)
    {
        if (command_equals(command, shell_aliases[i].name))
        {
            return shell_aliases[i].target;
        }
    }

    return command;
}

static int command_prefix_matches(const char *command,
                                  const char *prefix,
                                  unsigned int prefix_length)
{
    for (unsigned int i = 0; i < prefix_length; i++)
    {
        if (command[i] != prefix[i])
        {
            return 0;
        }
    }

    return 1;
}

static const char *command_arg(const char *command)
{
    while (*command && *command != ' ')
    {
        command++;
    }

    return skip_spaces(command);
}

static const struct shell_command *find_shell_command(const char *name)
{
    for (unsigned int i = 0; i < shell_command_count; i++)
    {
        if (string_equals(name, shell_commands[i].name))
        {
            return &shell_commands[i];
        }
    }

    return 0;
}

static const struct shell_command *find_shell_command_token(const char *text)
{
    for (unsigned int i = 0; i < shell_command_count; i++)
    {
        if (command_equals(text, shell_commands[i].name))
        {
            return &shell_commands[i];
        }
    }

    return 0;
}

static const struct shell_alias *find_shell_alias(const char *name)
{
    for (unsigned int i = 0; i < shell_alias_count; i++)
    {
        if (string_equals(name, shell_aliases[i].name))
        {
            return &shell_aliases[i];
        }
    }

    return 0;
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

static int command_line_is_completion_target(void)
{
    for (unsigned int i = 0; i < command_length; i++)
    {
        if (command_buffer[i] == ' ')
        {
            return 0;
        }
    }

    return command_cursor == command_length;
}

static void shell_redraw_current_line(void)
{
    shell_prompt();

    for (unsigned int i = 0; i < command_length; i++)
    {
        kprintf("%c", command_buffer[i]);
    }

    command_cursor = command_length;
    rendered_length = command_length;
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
        command_buffer[0] = '\0';
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
    command_buffer[command_length] = '\0';
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

static void shell_insert_text(const char *text)
{
    while (*text)
    {
        shell_insert_char(*text);
        text++;
    }
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
    command_buffer[command_length] = '\0';
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
    command_buffer[command_length] = '\0';
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

static void shell_autocomplete(void)
{
    const char *match = 0;
    unsigned int matches = 0;

    if (!command_line_is_completion_target())
    {
        return;
    }

    for (unsigned int i = 0; i < shell_command_count; i++)
    {
        if (command_prefix_matches(shell_commands[i].name,
                                   command_buffer,
                                   command_length))
        {
            match = shell_commands[i].name;
            matches++;
        }
    }

    for (unsigned int i = 0; i < shell_alias_count; i++)
    {
        if (command_prefix_matches(shell_aliases[i].name,
                                   command_buffer,
                                   command_length))
        {
            match = shell_aliases[i].name;
            matches++;
        }
    }

    if (matches != 1 || !match)
    {
        if (matches > 1)
        {
            kprintf("\nMatches:");

            for (unsigned int i = 0; i < shell_command_count; i++)
            {
                if (command_prefix_matches(shell_commands[i].name,
                                           command_buffer,
                                           command_length))
                {
                    kprintf(" %s", shell_commands[i].name);
                }
            }

            for (unsigned int i = 0; i < shell_alias_count; i++)
            {
                if (command_prefix_matches(shell_aliases[i].name,
                                           command_buffer,
                                           command_length))
                {
                    kprintf(" %s", shell_aliases[i].name);
                }
            }

            kprintf("\n");
            shell_redraw_current_line();
        }

        return;
    }

    shell_insert_text(match + command_length);
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
    for (unsigned int i = 0; i < shell_command_count; i++)
    {
        kprintf("  %s", shell_commands[i].name);

        for (unsigned int j = string_length(shell_commands[i].name);
             j < 8;
             j++)
        {
            kprintf(" ");
        }

        kprintf(" %s\n", shell_commands[i].description);
    }

    kprintf("Aliases:\n");
    for (unsigned int i = 0; i < shell_alias_count; i++)
    {
        kprintf("  %s", shell_aliases[i].name);

        for (unsigned int j = string_length(shell_aliases[i].name);
             j < 8;
             j++)
        {
            kprintf(" ");
        }

        kprintf(" %s -> %s\n",
                shell_aliases[i].description,
                shell_aliases[i].target);
    }
}

static void shell_command_help(const char *name)
{
    const struct shell_alias *alias = find_shell_alias(name);
    const struct shell_command *command;

    if (alias)
    {
        kprintf("Alias: %s -> %s\n", alias->name, alias->target);
        name = alias->target;
    }

    command = find_shell_command(name);
    if (!command)
    {
        kprintf("No help for: %s\n", name);
        kprintf("Type 'help' for commands\n");
        return;
    }

    kprintf("Command: %s\n", command->name);
    kprintf("Usage: %s\n", command->usage);
    kprintf("About: %s\n", command->description);
    if (command->example)
    {
        kprintf("Example: %s\n", command->example);
    }
}

static void shell_usage(const char *name)
{
    const struct shell_command *command = find_shell_command(name);

    if (command)
    {
        kprintf("Usage: %s\n", command->usage);
    }
}

static void shell_execute(const char *command)
{
    const char *effective_command = command_alias_target(command);
    const char *arg = command_arg(command);
    const struct shell_command *selected_command;

    if (command[0] == '\0')
    {
        return;
    }

    selected_command = find_shell_command(effective_command);
    if (!selected_command)
    {
        selected_command = find_shell_command_token(effective_command);
    }

    if (*arg != '\0' &&
        selected_command &&
        !string_equals(selected_command->name, "help") &&
        !string_equals(selected_command->name, "vmwalk"))
    {
        kprintf("Unexpected argument: %s\n", arg);
        shell_usage(selected_command->name);
        return;
    }

    if (command_equals(effective_command, "help"))
    {
        if (*arg == '\0')
        {
            shell_help();
        }
        else
        {
            shell_command_help(arg);
        }
    }
    else if (string_equals(effective_command, "version"))
    {
        kprintf("%s %s (%s)\n",
                SATURNOS_NAME,
                SATURNOS_VERSION,
                SATURNOS_CODENAME);
    }
    else if (string_equals(effective_command, "tasks"))
    {
        scheduler_dump_tasks();
    }
    else if (string_equals(effective_command, "mem"))
    {
        pmm_dump_stats();
    }
    else if (string_equals(effective_command, "heap"))
    {
        heap_dump_stats();
    }
    else if (string_equals(effective_command, "heaptest"))
    {
        heap_self_test();
    }
    else if (string_equals(effective_command, "vm"))
    {
        vm_dump_plan();
    }
    else if (command_equals(effective_command, "vmwalk"))
    {
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
                shell_command_help("vmwalk");
            }
        }
    }
    else if (string_equals(effective_command, "ticks"))
    {
        kprintf("scheduler ticks=%d timer irqs=%d\n",
                (int)scheduler_get_ticks(),
                (int)timer_get_irq_ticks());
    }
    else if (string_equals(effective_command, "clear"))
    {
        framebuffer_console_init();
    }
    else if (string_equals(effective_command, "panic"))
    {
        exception_test();
    }
    else if (string_equals(effective_command, "fault"))
    {
        exception_test_page_fault();
    }
    else
    {
        if (*arg != '\0' && selected_command)
        {
            kprintf("Unexpected argument: %s\n", arg);
            shell_usage(effective_command);
        }
        else
        {
            kprintf("Unknown command: %s\n", command);
            kprintf("Type 'help' for commands\n");
        }
    }
}

void shell_init(void)
{
    command_length = 0;
    command_buffer[0] = '\0';
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
        command_buffer[0] = '\0';
        command_cursor = 0;
        rendered_length = 0;
        escape_state = 0;
        history_view = history_count;
        shell_prompt();
        return;
    }

    if (c == '\t')
    {
        shell_autocomplete();
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
