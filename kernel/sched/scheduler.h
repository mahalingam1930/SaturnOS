#ifndef SCHEDULER_H
#define SCHEDULER_H

#define SCHED_MAX_TASKS 4

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
};

void scheduler_init(void);
void scheduler_tick(void);
const struct task *scheduler_current_task(void);
unsigned long scheduler_get_ticks(void);

#endif
