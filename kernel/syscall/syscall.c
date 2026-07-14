#include "syscall.h"
#include "kprintf.h"
#include "scheduler.h"

struct syscall_stats
{
    unsigned long total;
    unsigned long handled;
    unsigned long rejected;
    unsigned long write_calls;
    unsigned long exit_calls;
    unsigned long yield_calls;
    unsigned long last_number;
    unsigned long last_arg0;
    unsigned long last_arg1;
    unsigned long last_arg2;
    long last_result;
};

static struct syscall_stats stats;

const char *syscall_name(unsigned long number)
{
    switch (number)
    {
        case SYSCALL_WRITE:
            return "write";
        case SYSCALL_EXIT:
            return "exit";
        case SYSCALL_YIELD:
            return "yield";
        default:
            return "unknown";
    }
}

long syscall_dispatch(unsigned long number,
                      unsigned long arg0,
                      unsigned long arg1,
                      unsigned long arg2,
                      unsigned long arg3)
{
    long result = 0;
    (void)arg3;

    stats.total++;
    stats.last_number = number;
    stats.last_arg0 = arg0;
    stats.last_arg1 = arg1;
    stats.last_arg2 = arg2;

    switch (number)
    {
        case SYSCALL_WRITE:
            stats.write_calls++;
            stats.handled++;
            result = (long)arg2;
            break;
        case SYSCALL_EXIT:
            stats.exit_calls++;
            stats.handled++;
            result = 0;
            break;
        case SYSCALL_YIELD:
            stats.yield_calls++;
            stats.handled++;
            scheduler_yield();
            result = 0;
            break;
        default:
            stats.rejected++;
            result = -1;
            break;
    }

    stats.last_result = result;
    return result;
}

void syscall_dump_stats(void)
{
    kprintf("Syscalls:\n");
    kprintf("  %d %s args: fd, buffer, length\n",
            (int)SYSCALL_WRITE,
            syscall_name(SYSCALL_WRITE));
    kprintf("  %d %s args: code\n",
            (int)SYSCALL_EXIT,
            syscall_name(SYSCALL_EXIT));
    kprintf("  %d %s args: none\n",
            (int)SYSCALL_YIELD,
            syscall_name(SYSCALL_YIELD));
    kprintf("Stats:\n");
    kprintf("  total=%d handled=%d rejected=%d\n",
            (int)stats.total,
            (int)stats.handled,
            (int)stats.rejected);
    kprintf("  write=%d exit=%d yield=%d\n",
            (int)stats.write_calls,
            (int)stats.exit_calls,
            (int)stats.yield_calls);
    kprintf("  last=%d (%s) arg0=0x%x arg1=0x%x arg2=0x%x result=%d\n",
            (int)stats.last_number,
            syscall_name(stats.last_number),
            (unsigned int)stats.last_arg0,
            (unsigned int)stats.last_arg1,
            (unsigned int)stats.last_arg2,
            (int)stats.last_result);
}
