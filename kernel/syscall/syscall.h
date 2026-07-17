#ifndef SYSCALL_H
#define SYSCALL_H

#define SYSCALL_WRITE 1UL
#define SYSCALL_EXIT 2UL
#define SYSCALL_YIELD 3UL
#define SYSCALL_OPEN 4UL
#define SYSCALL_READ 5UL
#define SYSCALL_CLOSE 6UL
#define SYSCALL_CREATE 7UL
#define SYSCALL_SEEK 8UL
#define SYSCALL_WAIT 9UL
#define SYSCALL_SPAWN 10UL
#define SYSCALL_TERMINATE 11UL
#define SYSCALL_SLEEP 12UL
#define SYSCALL_MONOTONIC_MS 13UL
#define SYSCALL_GETPID 14UL
#define SYSCALL_GETPPID 15UL
#define SYSCALL_SYSTEM_INFO 16UL
#define SYSCALL_RANDOM 17UL
#define SYSCALL_PROCESS_STATUS 18UL
#define SYSCALL_DIRECTORY_LIST 19UL
#define SYSCALL_MKDIR 20UL
#define SYSCALL_REMOVE 21UL

#define SYSCALL_DIRECTORY_PATH_MAX 48UL
#define SYSCALL_WAIT_NOHANG 1UL

struct syscall_system_info
{
    unsigned long version_major;
    unsigned long version_minor;
    unsigned long version_patch;
    unsigned long page_size;
    unsigned long total_pages;
    unsigned long free_pages;
    unsigned long scheduler_ticks;
    unsigned long task_count;
    unsigned long task_capacity;
};

struct syscall_process_status
{
    unsigned long pid;
    unsigned long parent_pid;
    unsigned long state;
    unsigned long switches;
    unsigned long run_ticks;
    unsigned long yields;
    unsigned long preemptions;
    unsigned long exit_code;
    unsigned long succeeded;
};

struct syscall_directory_entry
{
    char path[SYSCALL_DIRECTORY_PATH_MAX];
    unsigned long size;
    unsigned long kind;
};

long syscall_dispatch(unsigned long number,
                      unsigned long arg0,
                      unsigned long arg1,
                      unsigned long arg2,
                      unsigned long arg3);
const char *syscall_name(unsigned long number);
void syscall_dump_stats(void);

#endif
