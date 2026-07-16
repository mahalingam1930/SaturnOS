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
#define SYSCALL_WAIT_NOHANG 1UL

long syscall_dispatch(unsigned long number,
                      unsigned long arg0,
                      unsigned long arg1,
                      unsigned long arg2,
                      unsigned long arg3);
const char *syscall_name(unsigned long number);
void syscall_dump_stats(void);

#endif
