#ifndef SYSCALL_H
#define SYSCALL_H

#define SYSCALL_WRITE 1UL
#define SYSCALL_EXIT 2UL
#define SYSCALL_YIELD 3UL

long syscall_dispatch(unsigned long number,
                      unsigned long arg0,
                      unsigned long arg1,
                      unsigned long arg2,
                      unsigned long arg3);
const char *syscall_name(unsigned long number);
void syscall_dump_stats(void);

#endif
