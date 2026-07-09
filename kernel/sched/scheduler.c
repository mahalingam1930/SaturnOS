#include "scheduler.h"
#include "irq.h"
#include "kprintf.h"
#include "timer.h"

static void thread_a(void);
static void thread_b(void);
static void thread_trampoline(void);

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

        if (threads_started && candidate == 0)
        {
            continue;
        }

        if (tasks[candidate].state != TASK_ZOMBIE)
        {
            return candidate;
        }
    }

    return current_task;
}

static void scheduler_add_task(int pid, const char *name, void (*entry)(void))
{
    unsigned long stack_top;

    if (task_count >= SCHED_MAX_TASKS)
    {
        return;
    }

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
        tasks[task_count].context.lr = (unsigned long)thread_trampoline;
    }

    task_count++;
}

void scheduler_init(void)
{
    task_count = 0;
    current_task = 0;
    threads_started = 0;
    scheduler_ticks = 0;

    scheduler_add_task(0, "kernel", 0);
    scheduler_add_task(1, "thread-a", thread_a);
    scheduler_add_task(2, "thread-b", thread_b);

    tasks[current_task].state = TASK_RUNNING;

    kprintf("Scheduler initialized with %d tasks\n", task_count);
    scheduler_dump_tasks();
}

void scheduler_tick(void)
{
    int previous_task = current_task;

    scheduler_ticks++;

    tasks[current_task].state = TASK_READY;
    current_task = scheduler_next_runnable_task();

    tasks[current_task].state = TASK_RUNNING;

    kprintf("Scheduler tick %d: task %d -> task %d (%s)\n",
            (int)scheduler_ticks,
            tasks[previous_task].pid,
            tasks[current_task].pid,
            tasks[current_task].name);
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

    kprintf("Switching task %d -> task %d (%s)\n",
            tasks[previous_task].pid,
            tasks[next_task].pid,
            tasks[next_task].name);

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

    kprintf("Preempt tick %d: task %d -> task %d (%s)\n",
            (int)scheduler_ticks,
            tasks[previous_task].pid,
            tasks[next_task].pid,
            tasks[next_task].name);

    cpu_switch_to(&tasks[previous_task].context, &tasks[next_task].context);
}

void scheduler_exit(void)
{
    kprintf("Task %d (%s) exited\n",
            tasks[current_task].pid,
            tasks[current_task].name);

    tasks[current_task].state = TASK_ZOMBIE;
    scheduler_yield();

    while (1)
    {
    }
}

void scheduler_start_threads(void)
{
    kprintf("Starting preemptive kernel threads...\n");
    threads_started = 1;
    scheduler_yield();
}

void scheduler_dump_tasks(void)
{
    int i;

    for (i = 0; i < task_count; i++)
    {
        kprintf("Task %d: %s state=%d context_slot=%d\n",
                tasks[i].pid,
                tasks[i].name,
                tasks[i].state,
                i);
    }
}

static void thread_trampoline(void)
{
    void (*entry)(void) = tasks[current_task].entry;

    irq_enable();

    if (entry)
    {
        entry();
    }

    scheduler_exit();
}

static void thread_a(void)
{
    int counter = 0;

    while (counter < 4)
    {
        counter++;
        kprintf("Thread A iteration %d\n", counter);
        timer_sleep_ms(250);
    }

    kprintf("Thread A returning\n");
}

static void thread_b(void)
{
    int counter = 0;

    while (1)
    {
        counter++;
        kprintf("Thread B iteration %d\n", counter);
        timer_sleep_ms(250);
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
