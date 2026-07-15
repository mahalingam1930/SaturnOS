#include "syscall.h"
#include "console.h"
#include "kprintf.h"
#include "scheduler.h"
#include "user.h"
#include "vfs.h"

#define SYSCALL_STDOUT 1UL
#define SYSCALL_STDERR 2UL
#define SYSCALL_WRITE_MAX 128UL
#define SYSCALL_READ_MAX 128UL
#define SYSCALL_ERR_INVAL -1L
#define SYSCALL_ERR_FAULT -2L

struct syscall_stats
{
    unsigned long total;
    unsigned long handled;
    unsigned long rejected;
    unsigned long write_calls;
    unsigned long exit_calls;
    unsigned long yield_calls;
    unsigned long open_calls;
    unsigned long read_calls;
    unsigned long close_calls;
    unsigned long create_calls;
    unsigned long seek_calls;
    unsigned long wait_calls;
    unsigned long faults;
    unsigned long write_bytes;
    unsigned long file_write_bytes;
    unsigned long last_exit_code;
    unsigned long last_number;
    unsigned long last_arg0;
    unsigned long last_arg1;
    unsigned long last_arg2;
    long last_result;
};

static struct syscall_stats stats;

static const struct address_space *syscall_address_space(void)
{
    const struct address_space *space = user_mode_active_address_space();
    const struct task *task;

    if (space)
    {
        return space;
    }

    task = scheduler_current_task();
    if (task && task->memory.address_space &&
        task->memory.address_space->kind == ADDRESS_SPACE_USER)
    {
        return task->memory.address_space;
    }

    return 0;
}

static long syscall_write(unsigned long fd,
                          unsigned long buffer,
                          unsigned long length)
{
    const char *bytes = (const char *)buffer;
    const struct address_space *space;
    struct task *task;
    int slot;
    long result;

    if (length == 0)
    {
        return 0;
    }

    if (length > SYSCALL_WRITE_MAX)
    {
        return SYSCALL_ERR_INVAL;
    }

    space = syscall_address_space();
    if (!address_space_user_range_valid(space, buffer, length))
    {
        stats.faults++;
        return SYSCALL_ERR_FAULT;
    }

    if (fd == SYSCALL_STDOUT || fd == SYSCALL_STDERR)
    {
        for (unsigned long i = 0; i < length; i++)
        {
            console_putc(bytes[i]);
        }
        stats.write_bytes += length;
        return (long)length;
    }

    task = scheduler_current_task_mutable();
    if (!task || fd < 3 || fd >= 3 + TASK_MAX_FILES)
    {
        return SYSCALL_ERR_INVAL;
    }
    slot = (int)(fd - 3);
    if (!task->files[slot].used)
    {
        return SYSCALL_ERR_INVAL;
    }
    result = vfs_write(task->files[slot].path,
                       task->files[slot].offset,
                       bytes,
                       length);
    if (result > 0)
    {
        task->files[slot].offset += (unsigned long)result;
        stats.file_write_bytes += (unsigned long)result;
    }
    return result < 0 ? SYSCALL_ERR_INVAL : result;
}

static long syscall_open(unsigned long path_address,
                         unsigned long path_length,
                         int create)
{
    const struct address_space *space = syscall_address_space();
    struct task *task = scheduler_current_task_mutable();
    const char *path = (const char *)path_address;
    unsigned long file_size;
    int slot = -1;

    if (!task || !space || path_length == 0 ||
        path_length >= TASK_FILE_PATH_SIZE ||
        !address_space_user_range_valid(space, path_address, path_length))
    {
        stats.faults++;
        return SYSCALL_ERR_FAULT;
    }
    for (int i = 0; i < TASK_MAX_FILES; i++)
    {
        if (!task->files[i].used)
        {
            slot = i;
            break;
        }
    }
    if (slot < 0)
    {
        return SYSCALL_ERR_INVAL;
    }
    for (unsigned long i = 0; i < path_length; i++)
    {
        if (path[i] == '\0')
        {
            return SYSCALL_ERR_INVAL;
        }
        task->files[slot].path[i] = path[i];
    }
    task->files[slot].path[path_length] = '\0';
    if (create &&
        !vfs_create(task->files[slot].path, 0, 0) &&
        !vfs_truncate(task->files[slot].path, 0))
    {
        task->files[slot].path[0] = '\0';
        return SYSCALL_ERR_INVAL;
    }
    if (!vfs_file_data(task->files[slot].path, &file_size))
    {
        task->files[slot].path[0] = '\0';
        return SYSCALL_ERR_INVAL;
    }
    task->files[slot].used = 1;
    task->files[slot].offset = 0;
    return 3L + slot;
}

static long syscall_read(unsigned long fd,
                         unsigned long buffer,
                         unsigned long length)
{
    const struct address_space *space = syscall_address_space();
    struct task *task = scheduler_current_task_mutable();
    int slot;
    long result;

    if (!task || fd < 3 || fd >= 3 + TASK_MAX_FILES ||
        length > SYSCALL_READ_MAX)
    {
        return SYSCALL_ERR_INVAL;
    }
    if (length == 0)
    {
        return 0;
    }
    if (!address_space_user_writable_range_valid(space, buffer, length))
    {
        stats.faults++;
        return SYSCALL_ERR_FAULT;
    }
    slot = (int)(fd - 3);
    if (!task->files[slot].used)
    {
        return SYSCALL_ERR_INVAL;
    }
    result = vfs_read(task->files[slot].path,
                      task->files[slot].offset,
                      (void *)buffer,
                      length);
    if (result > 0)
    {
        task->files[slot].offset += (unsigned long)result;
    }
    return result < 0 ? SYSCALL_ERR_INVAL : result;
}

static long syscall_close(unsigned long fd)
{
    struct task *task = scheduler_current_task_mutable();
    int slot;

    if (!task || fd < 3 || fd >= 3 + TASK_MAX_FILES)
    {
        return SYSCALL_ERR_INVAL;
    }
    slot = (int)(fd - 3);
    if (!task->files[slot].used)
    {
        return SYSCALL_ERR_INVAL;
    }
    task->files[slot].used = 0;
    task->files[slot].path[0] = '\0';
    task->files[slot].offset = 0;
    return 0;
}

static long syscall_seek(unsigned long fd, unsigned long offset)
{
    struct task *task = scheduler_current_task_mutable();
    unsigned long file_size;
    int slot;

    if (!task || fd < 3 || fd >= 3 + TASK_MAX_FILES)
    {
        return SYSCALL_ERR_INVAL;
    }
    slot = (int)(fd - 3);
    if (!task->files[slot].used ||
        !vfs_file_data(task->files[slot].path, &file_size) ||
        offset > file_size)
    {
        return SYSCALL_ERR_INVAL;
    }
    task->files[slot].offset = offset;
    return (long)offset;
}

static long syscall_wait(unsigned long pid, unsigned long status_address)
{
    const struct address_space *space = syscall_address_space();
    struct task_wait_status *status =
        (struct task_wait_status *)status_address;

    if (!address_space_user_writable_range_valid(space,
                                                  status_address,
                                                  sizeof(*status)))
    {
        stats.faults++;
        return SYSCALL_ERR_FAULT;
    }
    return scheduler_wait_task_status((int)pid, status);
}

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
        case SYSCALL_OPEN:
            return "open";
        case SYSCALL_READ:
            return "read";
        case SYSCALL_CLOSE:
            return "close";
        case SYSCALL_CREATE:
            return "create";
        case SYSCALL_SEEK:
            return "seek";
        case SYSCALL_WAIT:
            return "wait";
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
            result = syscall_write(arg0, arg1, arg2);
            if (result < 0)
            {
                stats.rejected++;
            }
            else
            {
                stats.handled++;
            }
            break;
        case SYSCALL_EXIT:
            stats.exit_calls++;
            stats.handled++;
            stats.last_exit_code = arg0;
            user_mode_handle_exit_syscall(arg0);
            result = 0;
            break;
        case SYSCALL_YIELD:
            stats.yield_calls++;
            stats.handled++;
            scheduler_yield();
            result = 0;
            break;
        case SYSCALL_OPEN:
            stats.open_calls++;
            result = syscall_open(arg0, arg1, 0);
            break;
        case SYSCALL_READ:
            stats.read_calls++;
            result = syscall_read(arg0, arg1, arg2);
            break;
        case SYSCALL_CLOSE:
            stats.close_calls++;
            result = syscall_close(arg0);
            break;
        case SYSCALL_CREATE:
            stats.create_calls++;
            result = syscall_open(arg0, arg1, 1);
            break;
        case SYSCALL_SEEK:
            stats.seek_calls++;
            result = syscall_seek(arg0, arg1);
            break;
        case SYSCALL_WAIT:
            stats.wait_calls++;
            result = syscall_wait(arg0, arg1);
            break;
        default:
            stats.rejected++;
            result = -1;
            break;
    }

    if (number >= SYSCALL_OPEN && number <= SYSCALL_WAIT)
    {
        if (result < 0)
        {
            stats.rejected++;
        }
        else
        {
            stats.handled++;
        }
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
    kprintf("  %d %s args: path, length\n", (int)SYSCALL_OPEN,
            syscall_name(SYSCALL_OPEN));
    kprintf("  %d %s args: fd, buffer, length\n", (int)SYSCALL_READ,
            syscall_name(SYSCALL_READ));
    kprintf("  %d %s args: fd\n", (int)SYSCALL_CLOSE,
            syscall_name(SYSCALL_CLOSE));
    kprintf("  %d %s args: path, length\n", (int)SYSCALL_CREATE,
            syscall_name(SYSCALL_CREATE));
    kprintf("  %d %s args: fd, offset\n", (int)SYSCALL_SEEK,
            syscall_name(SYSCALL_SEEK));
    kprintf("  %d %s args: pid, status\n", (int)SYSCALL_WAIT,
            syscall_name(SYSCALL_WAIT));
    kprintf("Stats:\n");
    kprintf("  total=%d handled=%d rejected=%d\n",
            (int)stats.total,
            (int)stats.handled,
            (int)stats.rejected);
    kprintf("  write=%d exit=%d yield=%d\n",
            (int)stats.write_calls,
            (int)stats.exit_calls,
            (int)stats.yield_calls);
    kprintf("  open=%d read=%d close=%d create=%d seek=%d wait=%d\n",
            (int)stats.open_calls,
            (int)stats.read_calls,
            (int)stats.close_calls,
            (int)stats.create_calls,
            (int)stats.seek_calls,
            (int)stats.wait_calls);
    kprintf("  write_bytes=%d faults=%d last_exit=%d\n",
            (int)stats.write_bytes,
            (int)stats.faults,
            (int)stats.last_exit_code);
    kprintf("  file_write_bytes=%d\n", (int)stats.file_write_bytes);
    kprintf("  last=%d (%s) arg0=0x%x arg1=0x%x arg2=0x%x result=%d\n",
            (int)stats.last_number,
            syscall_name(stats.last_number),
            (unsigned int)stats.last_arg0,
            (unsigned int)stats.last_arg1,
            (unsigned int)stats.last_arg2,
            (int)stats.last_result);
}
