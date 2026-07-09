#include "scheduler.h"
#include "irq.h"
#include "kprintf.h"

#define SCHED_DEBUG 0

static void idle_task(void);
static void scheduler_exit_task(int task_id);
static void thread_trampoline(void);

static const char *scheduler_state_name(enum task_state state)
{
    switch (state)
    {
        case TASK_UNUSED:
            return "unused";
        case TASK_READY:
            return "ready";
        case TASK_RUNNING:
            return "running";
        case TASK_ZOMBIE:
            return "zombie";
        default:
            return "unknown";
    }
}

static void scheduler_clear_context(struct cpu_context *context)
{
    context->x19 = 0;
    context->x20 = 0;
    context->x21 = 0;
    context->x22 = 0;
    context->x23 = 0;
    context->x24 = 0;
    context->x25 = 0;
    context->x26 = 0;
    context->x27 = 0;
    context->x28 = 0;
    context->fp = 0;
    context->lr = 0;
    context->sp = 0;
}

static struct task tasks[SCHED_MAX_TASKS];
static unsigned char task_stacks[SCHED_MAX_TASKS][SCHED_STACK_SIZE] __attribute__((aligned(16)));
static struct cpu_context bootstrap_context;
static int current_task;
static int task_count;
static int threads_started;
static unsigned long scheduler_ticks;

static int scheduler_next_runnable_task(void)
{
    int i;
    int candidate;

    for (i = 1; i <= task_count; i++)
    {
        candidate = (current_task + i) % task_count;

        if (candidate != 0 && tasks[candidate].state != TASK_ZOMBIE)
        {
            return candidate;
        }
    }

    if (tasks[0].state != TASK_ZOMBIE)
    {
        return 0;
    }

    return current_task;
}

static int scheduler_add_task(const char *name, void (*entry)(void))
{
    unsigned long stack_top;
    int pid;

    if (task_count >= SCHED_MAX_TASKS)
    {
        return -1;
    }

    pid = task_count;

    tasks[task_count].pid = pid;
    tasks[task_count].name = name;
    tasks[task_count].state = TASK_READY;
    tasks[task_count].entry = entry;
    scheduler_clear_context(&tasks[task_count].context);

    if (entry)
    {
        stack_top = (unsigned long)&task_stacks[task_count][SCHED_STACK_SIZE];
        stack_top &= ~((unsigned long)0xF);

        tasks[task_count].context.sp = stack_top;

        if (pid == 0)
        {
            tasks[task_count].context.lr = (unsigned long)entry;
        }
        else
        {
            tasks[task_count].context.lr = (unsigned long)thread_trampoline;
        }
    }

    task_count++;

    return pid;
}

void scheduler_init(void)
{
    task_count = 0;
    current_task = 0;
    threads_started = 0;
    scheduler_ticks = 0;
    scheduler_clear_context(&bootstrap_context);

    scheduler_add_task("idle", idle_task);

    kprintf("Scheduler initialized with %d tasks\n", task_count);
    scheduler_dump_tasks();
}

int scheduler_create_kernel_thread(const char *name, void (*entry)(void))
{
    int pid = scheduler_add_task(name, entry);

    if (pid < 0)
    {
        kprintf("Failed to create task: %s\n", name);
        return -1;
    }

    kprintf("Created kernel thread %d: %s\n", pid, name);
    return pid;
}

void scheduler_tick(void)
{
    int previous_task = current_task;

    scheduler_ticks++;

    tasks[current_task].state = TASK_READY;
    current_task = scheduler_next_runnable_task();

    tasks[current_task].state = TASK_RUNNING;

    if (SCHED_DEBUG)
    {
        kprintf("Scheduler tick %d: task %d -> task %d (%s)\n",
                (int)scheduler_ticks,
                tasks[previous_task].pid,
                tasks[current_task].pid,
                tasks[current_task].name);
    }
}

void scheduler_yield(void)
{
    int previous_task;
    int next_task;

    if (task_count < 2)
    {
        return;
    }

    previous_task = current_task;
    next_task = scheduler_next_runnable_task();

    if (next_task == previous_task)
    {
        if (tasks[previous_task].state != TASK_ZOMBIE)
        {
            tasks[previous_task].state = TASK_RUNNING;
            return;
        }

        kprintf("No runnable task remains\n");
        while (1)
        {
        }
    }

    if (tasks[previous_task].state == TASK_RUNNING)
    {
        tasks[previous_task].state = TASK_READY;
    }

    tasks[next_task].state = TASK_RUNNING;
    current_task = next_task;

    if (SCHED_DEBUG)
    {
        kprintf("Switching task %d -> task %d (%s)\n",
                tasks[previous_task].pid,
                tasks[next_task].pid,
                tasks[next_task].name);
    }

    cpu_switch_to(&tasks[previous_task].context, &tasks[next_task].context);
}

void scheduler_preempt(void)
{
    int previous_task;
    int next_task;

    if (!threads_started || task_count < 2)
    {
        return;
    }

    scheduler_ticks++;

    previous_task = current_task;
    next_task = scheduler_next_runnable_task();

    if (next_task == previous_task)
    {
        return;
    }

    if (tasks[previous_task].state == TASK_RUNNING)
    {
        tasks[previous_task].state = TASK_READY;
    }

    tasks[next_task].state = TASK_RUNNING;
    current_task = next_task;

    if (SCHED_DEBUG)
    {
        kprintf("Preempt tick %d: task %d -> task %d (%s)\n",
                (int)scheduler_ticks,
                tasks[previous_task].pid,
                tasks[next_task].pid,
                tasks[next_task].name);
    }

    cpu_switch_to(&tasks[previous_task].context, &tasks[next_task].context);
}

void scheduler_exit(void)
{
    scheduler_exit_task(current_task);
}

static void scheduler_exit_task(int task_id)
{
    kprintf("Task %d (%s) exited\n",
            tasks[task_id].pid,
            tasks[task_id].name);

    tasks[task_id].state = TASK_ZOMBIE;
    current_task = task_id;
    scheduler_yield();

    while (1)
    {
    }
}

void scheduler_start_threads(void)
{
    kprintf("Starting preemptive kernel threads...\n");
    threads_started = 1;

    current_task = 0;
    tasks[current_task].state = TASK_RUNNING;

    cpu_switch_to(&bootstrap_context, &tasks[current_task].context);

    while (1)
    {
    }
}

void scheduler_dump_tasks(void)
{
    int i;

    kprintf("Scheduler state: ticks=%d tasks=%d current=%d\n",
            (int)scheduler_ticks,
            task_count,
            current_task);

    for (i = 0; i < task_count; i++)
    {
        kprintf("  task %d: %s state=%s\n",
                tasks[i].pid,
                tasks[i].name,
                scheduler_state_name(tasks[i].state));
    }
}

static void thread_trampoline(void)
{
    int task_id = current_task;
    void (*entry)(void) = tasks[task_id].entry;

    irq_enable();

    if (entry)
    {
        entry();
    }

    scheduler_exit_task(task_id);
}

static void idle_task(void)
{
    while (1)
    {
        __asm__ volatile("wfi");
    }
}

const struct task *scheduler_current_task(void)
{
    return &tasks[current_task];
}

unsigned long scheduler_get_ticks(void)
{
    return scheduler_ticks;
}
