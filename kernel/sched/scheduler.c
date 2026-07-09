#include "scheduler.h"
#include "kprintf.h"

static struct task tasks[SCHED_MAX_TASKS];
static int current_task;
static int task_count;
static unsigned long scheduler_ticks;

static void scheduler_add_task(int pid, const char *name)
{
    if (task_count >= SCHED_MAX_TASKS)
    {
        return;
    }

    tasks[task_count].pid = pid;
    tasks[task_count].name = name;
    tasks[task_count].state = TASK_READY;
    task_count++;
}

void scheduler_init(void)
{
    task_count = 0;
    current_task = 0;
    scheduler_ticks = 0;

    scheduler_add_task(0, "idle");
    scheduler_add_task(1, "kernel");

    tasks[current_task].state = TASK_RUNNING;

    kprintf("Scheduler initialized with %d tasks\n", task_count);
}

void scheduler_tick(void)
{
    int previous_task = current_task;

    scheduler_ticks++;

    tasks[current_task].state = TASK_READY;
    current_task = (current_task + 1) % task_count;
    tasks[current_task].state = TASK_RUNNING;

    kprintf("Scheduler tick %d: task %d -> task %d (%s)\n",
            (int)scheduler_ticks,
            tasks[previous_task].pid,
            tasks[current_task].pid,
            tasks[current_task].name);
}

const struct task *scheduler_current_task(void)
{
    return &tasks[current_task];
}

unsigned long scheduler_get_ticks(void)
{
    return scheduler_ticks;
}
