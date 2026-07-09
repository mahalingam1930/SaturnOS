#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "context.h"

#define SCHED_MAX_TASKS 4
#define SCHED_STACK_SIZE 4096

enum task_state
{
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING
};

struct task
{
    int pid;
    const char *name;
    enum task_state state;
    struct cpu_context context;
};

void scheduler_init(void);
void scheduler_tick(void);
void scheduler_yield(void);
void scheduler_start_threads(void);
void scheduler_dump_tasks(void);
const struct task *scheduler_current_task(void);
unsigned long scheduler_get_ticks(void);

#endif
