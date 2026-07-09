#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "context.h"

#define SCHED_MAX_TASKS 4
#define SCHED_STACK_SIZE 4096

enum task_state
{
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_ZOMBIE
};

struct task
{
    int pid;
    const char *name;
    enum task_state state;
    void (*entry)(void);
    struct cpu_context context;
};

void scheduler_init(void);
int scheduler_create_kernel_thread(const char *name, void (*entry)(void));
void scheduler_tick(void);
void scheduler_yield(void);
void scheduler_preempt(void);
void scheduler_exit(void);
void scheduler_start_threads(void);
void scheduler_dump_tasks(void);
const struct task *scheduler_current_task(void);
unsigned long scheduler_get_ticks(void);

#endif
