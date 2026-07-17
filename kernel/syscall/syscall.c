#include "syscall.h"
#include "console.h"
#include "kprintf.h"
#include "scheduler.h"
#include "user.h"
#include "vfs.h"
#include "mmu.h"
#include "timer.h"
#include "pmm.h"
#include "version.h"

#define SYSCALL_STDOUT 1UL
#define SYSCALL_STDERR 2UL
#define SYSCALL_WRITE_MAX 128UL
#define SYSCALL_READ_MAX 128UL
#define SYSCALL_SLEEP_MAX_MS 60000UL
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
    unsigned long spawn_calls;
    unsigned long terminate_calls;
    unsigned long sleep_calls;
    unsigned long monotonic_calls;
    unsigned long getpid_calls;
    unsigned long getppid_calls;
    unsigned long system_info_calls;
    unsigned long random_calls;
    unsigned long process_status_calls;
    unsigned long directory_list_calls;
    unsigned long mkdir_calls;
    unsigned long remove_calls;
    unsigned long rename_calls;
    unsigned long path_stat_calls;
    unsigned long last_random;
    unsigned long last_monotonic_ms;
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
static unsigned long random_state = 0x6d2b79f5UL;
static const struct address_space *syscall_address_space(void);

static void syscall_scheduler_yield(void)
{
    unsigned long elr;
    unsigned long spsr;

    if (!user_mode_active_address_space())
    {
        scheduler_yield();
        return;
    }
    __asm__ volatile("mrs %0, ELR_EL1" : "=r"(elr));
    __asm__ volatile("mrs %0, SPSR_EL1" : "=r"(spsr));
    scheduler_yield();
    const struct address_space *space = user_mode_active_address_space();
    if (space)
    {
        arm64_mmu_switch_ttbr0(space->target_root_table);
    }
    __asm__ volatile("msr ELR_EL1, %0" : : "r"(elr) : "memory");
    __asm__ volatile("msr SPSR_EL1, %0" : : "r"(spsr) : "memory");
    __asm__ volatile("isb" : : : "memory");
}

static long syscall_sleep(unsigned long milliseconds)
{
    unsigned long elr;
    unsigned long spsr;

    if (!milliseconds || milliseconds > SYSCALL_SLEEP_MAX_MS)
    {
        return SYSCALL_ERR_INVAL;
    }
    __asm__ volatile("mrs %0, ELR_EL1" : "=r"(elr));
    __asm__ volatile("mrs %0, SPSR_EL1" : "=r"(spsr));
    scheduler_sleep_ms(milliseconds);
    const struct address_space *space = user_mode_active_address_space();
    if (space)
    {
        arm64_mmu_switch_ttbr0(space->target_root_table);
    }
    __asm__ volatile("msr ELR_EL1, %0" : : "r"(elr) : "memory");
    __asm__ volatile("msr SPSR_EL1, %0" : : "r"(spsr) : "memory");
    __asm__ volatile("isb" : : : "memory");
    return 0;
}

static long syscall_system_info(unsigned long address)
{
    const struct address_space *space = syscall_address_space();
    struct syscall_system_info *info = (struct syscall_system_info *)address;
    if (!address_space_user_writable_range_valid(space, address, sizeof(*info)))
    {
        stats.faults++;
        return SYSCALL_ERR_FAULT;
    }
    info->version_major = SATURNOS_VERSION_MAJOR;
    info->version_minor = SATURNOS_VERSION_MINOR;
    info->version_patch = SATURNOS_VERSION_PATCH;
    info->page_size = PMM_PAGE_SIZE;
    info->total_pages = pmm_total_pages();
    info->free_pages = pmm_free_pages();
    info->scheduler_ticks = scheduler_get_ticks();
    info->task_count = scheduler_task_count();
    info->task_capacity = SCHED_MAX_TASKS;
    return 0;
}

static long syscall_random(void)
{
    const struct task *task = scheduler_current_task();
    unsigned long value = random_state;
    value ^= scheduler_get_ticks() + (task ? (unsigned long)task->pid : 0UL);
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    random_state = value;
    return (long)(value & 0x7fffffffUL);
}

static long syscall_process_status(unsigned long pid, unsigned long address)
{
    const struct address_space *space = syscall_address_space();
    struct syscall_process_status *status =
        (struct syscall_process_status *)address;
    const struct task *caller = scheduler_current_task();
    const struct task *target;
    if (!address_space_user_writable_range_valid(space, address,
                                                  sizeof(*status)))
    {
        stats.faults++;
        return SYSCALL_ERR_FAULT;
    }
    if (!caller)
    {
        return SYSCALL_ERR_INVAL;
    }
    if (pid >= SCHED_MAX_TASKS)
    {
        return SYSCALL_ERR_INVAL;
    }
    target = scheduler_task_by_pid(pid ? (int)pid : caller->pid);
    if (!target || (target->pid != caller->pid &&
                    target->parent_pid != caller->pid))
    {
        return SYSCALL_ERR_INVAL;
    }
    status->pid = (unsigned long)target->pid;
    status->parent_pid = target->parent_pid > 0 ?
        (unsigned long)target->parent_pid : 0UL;
    status->state = (unsigned long)target->state;
    status->switches = target->accounting.switches;
    status->run_ticks = target->accounting.run_ticks;
    status->yields = target->accounting.yields;
    status->preemptions = target->accounting.preemptions;
    status->exit_code = target->user_status.last_exit_code;
    status->succeeded = target->user_status.program_succeeded ? 1UL : 0UL;
    return 0;
}

static long syscall_directory_list(unsigned long index, unsigned long address)
{
    const struct address_space *space = syscall_address_space();
    struct syscall_directory_entry *output =
        (struct syscall_directory_entry *)address;
    struct vfs_entry entry;
    int result;
    if (!address_space_user_writable_range_valid(space, address,
                                                  sizeof(*output)))
    {
        stats.faults++;
        return SYSCALL_ERR_FAULT;
    }
    result = vfs_list(index, &entry);
    if (result <= 0)
    {
        return result;
    }
    for (unsigned long i = 0; i < sizeof(output->path); i++)
    {
        output->path[i] = entry.path[i];
    }
    output->size = entry.size;
    output->kind = entry.kind;
    return 1;
}

static long syscall_mkdir(unsigned long path_address,
                          unsigned long path_length)
{
    const struct address_space *space = syscall_address_space();
    const char *source = (const char *)path_address;
    char path[VFS_MAX_PATH];
    if (!path_length || path_length >= sizeof(path))
    {
        return SYSCALL_ERR_INVAL;
    }
    if (!address_space_user_range_valid(space, path_address, path_length))
    {
        stats.faults++;
        return SYSCALL_ERR_FAULT;
    }
    for (unsigned long i = 0; i < path_length; i++)
    {
        if (!source[i])
        {
            return SYSCALL_ERR_INVAL;
        }
        path[i] = source[i];
    }
    path[path_length] = '\0';
    return vfs_mkdir(path) ? 0 : SYSCALL_ERR_INVAL;
}

static long syscall_remove(unsigned long path_address,
                           unsigned long path_length)
{
    const struct address_space *space = syscall_address_space();
    const char *source = (const char *)path_address;
    char path[VFS_MAX_PATH];
    if (!path_length || path_length >= sizeof(path))
    {
        return SYSCALL_ERR_INVAL;
    }
    if (!address_space_user_range_valid(space, path_address, path_length))
    {
        stats.faults++;
        return SYSCALL_ERR_FAULT;
    }
    for (unsigned long i = 0; i < path_length; i++)
    {
        if (!source[i])
        {
            return SYSCALL_ERR_INVAL;
        }
        path[i] = source[i];
    }
    path[path_length] = '\0';
    return vfs_remove(path) ? 0 : SYSCALL_ERR_INVAL;
}

static long syscall_rename(unsigned long old_address,
                           unsigned long old_length,
                           unsigned long new_address,
                           unsigned long new_length)
{
    const struct address_space *space = syscall_address_space();
    const char *old_source = (const char *)old_address;
    const char *new_source = (const char *)new_address;
    char old_path[VFS_MAX_PATH];
    char new_path[VFS_MAX_PATH];
    if (!old_length || old_length >= sizeof(old_path) ||
        !new_length || new_length >= sizeof(new_path))
    {
        return SYSCALL_ERR_INVAL;
    }
    if (!address_space_user_range_valid(space, old_address, old_length) ||
        !address_space_user_range_valid(space, new_address, new_length))
    {
        stats.faults++;
        return SYSCALL_ERR_FAULT;
    }
    for (unsigned long i = 0; i < old_length; i++)
    {
        if (!old_source[i])
        {
            return SYSCALL_ERR_INVAL;
        }
        old_path[i] = old_source[i];
    }
    for (unsigned long i = 0; i < new_length; i++)
    {
        if (!new_source[i])
        {
            return SYSCALL_ERR_INVAL;
        }
        new_path[i] = new_source[i];
    }
    old_path[old_length] = '\0';
    new_path[new_length] = '\0';
    return vfs_rename(old_path, new_path) ? 0 : SYSCALL_ERR_INVAL;
}

static long syscall_path_stat(unsigned long path_address,
                              unsigned long path_length,
                              unsigned long output_address)
{
    const struct address_space *space = syscall_address_space();
    const char *source = (const char *)path_address;
    struct syscall_directory_entry *output =
        (struct syscall_directory_entry *)output_address;
    struct vfs_entry entry;
    char path[VFS_MAX_PATH];
    if (!path_length || path_length >= sizeof(path))
    {
        return SYSCALL_ERR_INVAL;
    }
    if (!address_space_user_range_valid(space, path_address, path_length) ||
        !address_space_user_writable_range_valid(space, output_address,
                                                  sizeof(*output)))
    {
        stats.faults++;
        return SYSCALL_ERR_FAULT;
    }
    for (unsigned long i = 0; i < path_length; i++)
    {
        if (!source[i])
        {
            return SYSCALL_ERR_INVAL;
        }
        path[i] = source[i];
    }
    path[path_length] = '\0';
    if (!vfs_stat(path, &entry))
    {
        return SYSCALL_ERR_INVAL;
    }
    for (unsigned long i = 0; i < sizeof(output->path); i++)
    {
        output->path[i] = entry.path[i];
    }
    output->size = entry.size;
    output->kind = entry.kind;
    return 0;
}

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

static long syscall_wait(unsigned long pid,
                         unsigned long status_address,
                         unsigned long options)
{
    const struct address_space *space = syscall_address_space();
    struct task_wait_status *status =
        (struct task_wait_status *)status_address;

    if (options & ~SYSCALL_WAIT_NOHANG)
    {
        return SYSCALL_ERR_INVAL;
    }
    if (!address_space_user_writable_range_valid(space,
                                                  status_address,
                                                  sizeof(*status)))
    {
        stats.faults++;
        return SYSCALL_ERR_FAULT;
    }
    long result;
    do
    {
        result = scheduler_wait_task_status((int)pid, status);
        if (result == 0 && !(options & SYSCALL_WAIT_NOHANG))
        {
            syscall_scheduler_yield();
        }
    } while (result == 0 && !(options & SYSCALL_WAIT_NOHANG));
    return result;
}

static long syscall_spawn(unsigned long path_address,
                          unsigned long path_length,
                          unsigned long argument_address,
                          unsigned long argument_length)
{
    const struct address_space *space = syscall_address_space();
    const char *source = (const char *)path_address;
    char path[TASK_FILE_PATH_SIZE];
    char argument[TASK_USER_ARGUMENT_MAX + 1UL];
    int pid;

    if (!space || !path_length || path_length >= sizeof(path) ||
        !address_space_user_range_valid(space, path_address, path_length))
    {
        stats.faults++;
        return SYSCALL_ERR_FAULT;
    }
    for (unsigned long i = 0; i < path_length; i++)
    {
        if (!source[i]) return SYSCALL_ERR_INVAL;
        path[i] = source[i];
    }
    path[path_length] = '\0';
    if (argument_length > TASK_USER_ARGUMENT_MAX ||
        (argument_length &&
         !address_space_user_range_valid(space,
                                         argument_address,
                                         argument_length)))
    {
        stats.faults++;
        return SYSCALL_ERR_FAULT;
    }
    for (unsigned long i = 0; i < argument_length; i++)
    {
        argument[i] = ((const char *)argument_address)[i];
    }
    argument[argument_length] = '\0';
    pid = scheduler_create_user_task_from_image("user-child",
                                                path,
                                                argument_length ? argument : 0);
    struct task *parent = scheduler_current_task_mutable();
    if (pid < 0 || !parent ||
        !scheduler_set_task_parent(pid, parent->pid) ||
        scheduler_unblock_user_task(pid) < 0)
    {
        return SYSCALL_ERR_INVAL;
    }
    return pid;
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
        case SYSCALL_SPAWN:
            return "spawn";
        case SYSCALL_TERMINATE:
            return "terminate";
        case SYSCALL_SLEEP:
            return "sleep";
        case SYSCALL_MONOTONIC_MS:
            return "monotonic_ms";
        case SYSCALL_GETPID:
            return "getpid";
        case SYSCALL_GETPPID:
            return "getppid";
        case SYSCALL_SYSTEM_INFO:
            return "system_info";
        case SYSCALL_RANDOM:
            return "random";
        case SYSCALL_PROCESS_STATUS:
            return "process_status";
        case SYSCALL_DIRECTORY_LIST:
            return "directory_list";
        case SYSCALL_MKDIR:
            return "mkdir";
        case SYSCALL_REMOVE:
            return "remove";
        case SYSCALL_RENAME:
            return "rename";
        case SYSCALL_PATH_STAT:
            return "path_stat";
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
            syscall_scheduler_yield();
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
            result = syscall_wait(arg0, arg1, arg2);
            break;
        case SYSCALL_SPAWN:
            stats.spawn_calls++;
            result = syscall_spawn(arg0, arg1, arg2, arg3);
            break;
        case SYSCALL_TERMINATE:
            stats.terminate_calls++;
            result = scheduler_terminate_child((int)arg0, arg1) ? 0 :
                SYSCALL_ERR_INVAL;
            break;
        case SYSCALL_SLEEP:
            stats.sleep_calls++;
            result = syscall_sleep(arg0);
            break;
        case SYSCALL_MONOTONIC_MS:
            stats.monotonic_calls++;
            result = (long)(scheduler_get_ticks() * timer_get_interval_ms());
            stats.last_monotonic_ms = (unsigned long)result;
            break;
        case SYSCALL_GETPID:
        {
            const struct task *task = scheduler_current_task();
            stats.getpid_calls++;
            result = task ? task->pid : SYSCALL_ERR_INVAL;
            break;
        }
        case SYSCALL_GETPPID:
        {
            const struct task *task = scheduler_current_task();
            stats.getppid_calls++;
            result = task ? (task->parent_pid > 0 ? task->parent_pid : 0) :
                SYSCALL_ERR_INVAL;
            break;
        }
        case SYSCALL_SYSTEM_INFO:
            stats.system_info_calls++;
            result = syscall_system_info(arg0);
            break;
        case SYSCALL_RANDOM:
            stats.random_calls++;
            result = syscall_random();
            stats.last_random = (unsigned long)result;
            break;
        case SYSCALL_PROCESS_STATUS:
            stats.process_status_calls++;
            result = syscall_process_status(arg0, arg1);
            break;
        case SYSCALL_DIRECTORY_LIST:
            stats.directory_list_calls++;
            result = syscall_directory_list(arg0, arg1);
            break;
        case SYSCALL_MKDIR:
            stats.mkdir_calls++;
            result = syscall_mkdir(arg0, arg1);
            break;
        case SYSCALL_REMOVE:
            stats.remove_calls++;
            result = syscall_remove(arg0, arg1);
            break;
        case SYSCALL_RENAME:
            stats.rename_calls++;
            result = syscall_rename(arg0, arg1, arg2, arg3);
            break;
        case SYSCALL_PATH_STAT:
            stats.path_stat_calls++;
            result = syscall_path_stat(arg0, arg1, arg2);
            break;
        default:
            stats.rejected++;
            result = -1;
            break;
    }

    if (number >= SYSCALL_OPEN && number <= SYSCALL_PATH_STAT)
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
    kprintf("  %d %s args: pid, status, options\n", (int)SYSCALL_WAIT,
            syscall_name(SYSCALL_WAIT));
    kprintf("  %d %s args: path, length, arguments, length\n",
            (int)SYSCALL_SPAWN,
            syscall_name(SYSCALL_SPAWN));
    kprintf("  %d %s args: pid, code\n", (int)SYSCALL_TERMINATE,
            syscall_name(SYSCALL_TERMINATE));
    kprintf("  %d %s args: milliseconds\n", (int)SYSCALL_SLEEP,
            syscall_name(SYSCALL_SLEEP));
    kprintf("  %d %s args: none\n", (int)SYSCALL_MONOTONIC_MS,
            syscall_name(SYSCALL_MONOTONIC_MS));
    kprintf("  %d %s args: none\n", (int)SYSCALL_GETPID,
            syscall_name(SYSCALL_GETPID));
    kprintf("  %d %s args: none\n", (int)SYSCALL_GETPPID,
            syscall_name(SYSCALL_GETPPID));
    kprintf("  %d %s args: info buffer\n", (int)SYSCALL_SYSTEM_INFO,
            syscall_name(SYSCALL_SYSTEM_INFO));
    kprintf("  %d %s args: none\n", (int)SYSCALL_RANDOM,
            syscall_name(SYSCALL_RANDOM));
    kprintf("  %d %s args: pid, status buffer\n",
            (int)SYSCALL_PROCESS_STATUS,
            syscall_name(SYSCALL_PROCESS_STATUS));
    kprintf("  %d %s args: index, entry buffer\n",
            (int)SYSCALL_DIRECTORY_LIST,
            syscall_name(SYSCALL_DIRECTORY_LIST));
    kprintf("  %d %s args: path, length\n", (int)SYSCALL_MKDIR,
            syscall_name(SYSCALL_MKDIR));
    kprintf("  %d %s args: path, length\n", (int)SYSCALL_REMOVE,
            syscall_name(SYSCALL_REMOVE));
    kprintf("  %d %s args: old path, length, new path, length\n",
            (int)SYSCALL_RENAME, syscall_name(SYSCALL_RENAME));
    kprintf("  %d %s args: path, length, status buffer\n",
            (int)SYSCALL_PATH_STAT, syscall_name(SYSCALL_PATH_STAT));
    kprintf("Stats:\n");
    kprintf("  total=%d handled=%d rejected=%d\n",
            (int)stats.total,
            (int)stats.handled,
            (int)stats.rejected);
    kprintf("  write=%d exit=%d yield=%d\n",
            (int)stats.write_calls,
            (int)stats.exit_calls,
            (int)stats.yield_calls);
    kprintf("  open=%d read=%d close=%d create=%d seek=%d wait=%d spawn=%d terminate=%d sleep=%d time=%d\n",
            (int)stats.open_calls,
            (int)stats.read_calls,
            (int)stats.close_calls,
            (int)stats.create_calls,
            (int)stats.seek_calls,
            (int)stats.wait_calls,
            (int)stats.spawn_calls,
            (int)stats.terminate_calls,
            (int)stats.sleep_calls,
            (int)stats.monotonic_calls);
    kprintf("  write_bytes=%d faults=%d last_exit=%d\n",
            (int)stats.write_bytes,
            (int)stats.faults,
            (int)stats.last_exit_code);
    kprintf("  file_write_bytes=%d\n", (int)stats.file_write_bytes);
    kprintf("  monotonic_ms=%d\n", (int)stats.last_monotonic_ms);
    kprintf("  identity getpid=%d getppid=%d\n",
            (int)stats.getpid_calls,
            (int)stats.getppid_calls);
    kprintf("  system_info=%d\n", (int)stats.system_info_calls);
    kprintf("  random=%d last_random=%d\n",
            (int)stats.random_calls,
            (int)stats.last_random);
    kprintf("  process_status=%d\n", (int)stats.process_status_calls);
    kprintf("  directory_list=%d\n", (int)stats.directory_list_calls);
    kprintf("  mkdir=%d\n", (int)stats.mkdir_calls);
    kprintf("  remove=%d\n", (int)stats.remove_calls);
    kprintf("  rename=%d\n", (int)stats.rename_calls);
    kprintf("  path_stat=%d\n", (int)stats.path_stat_calls);
    kprintf("  last=%d (%s) arg0=0x%x arg1=0x%x arg2=0x%x result=%d\n",
            (int)stats.last_number,
            syscall_name(stats.last_number),
            (unsigned int)stats.last_arg0,
            (unsigned int)stats.last_arg1,
            (unsigned int)stats.last_arg2,
            (int)stats.last_result);
}
