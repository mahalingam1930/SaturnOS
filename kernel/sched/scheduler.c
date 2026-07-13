#include "scheduler.h"
#include "config.h"
#include "cpu.h"
#include "irq.h"
#include "kprintf.h"
#include "user.h"

static void idle_task(void);
static void scheduler_exit_task(int task_id);
static void thread_trampoline(void);

static struct address_space user_demo_address_space;
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
        case TASK_BLOCKED:
            return "blocked";
        case TASK_ELIGIBLE:
            return "eligible";
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

static void scheduler_clear_memory(struct task_memory *memory)
{
    memory->address_space = 0;
    memory->stack_start = 0;
    memory->stack_end = 0;
    memory->guard_low_start = 0;
    memory->guard_low_end = 0;
    memory->guard_high_start = 0;
    memory->guard_high_end = 0;
}

static void scheduler_clear_el0(struct task_el0_state *el0)
{
    el0->pc = 0;
    el0->sp = 0;
    el0->spsr = 0;
    el0->ready = 0;
}

static void scheduler_init_task_memory(struct task *task, int pid)
{
    task->memory.address_space = address_space_kernel();
    task->memory.stack_start = scheduler_stack_start((unsigned long)pid);
    task->memory.stack_end = scheduler_stack_end((unsigned long)pid);
    task->memory.guard_low_start =
        scheduler_stack_guard_start((unsigned long)pid);
    task->memory.guard_low_end =
        scheduler_stack_guard_end((unsigned long)pid);
    task->memory.guard_high_start =
        scheduler_stack_guard_start((unsigned long)pid + 1);
    task->memory.guard_high_end =
        scheduler_stack_guard_end((unsigned long)pid + 1);
}

static void scheduler_init_task_el0(struct task *task)
{
    struct address_space *space = task->memory.address_space;

    scheduler_clear_el0(&task->el0);

    if (!space || space->kind != ADDRESS_SPACE_USER)
    {
        return;
    }

    if (!space->user_mappings_ready || !space->user_execute_ready)
    {
        return;
    }

    task->el0.pc = space->user_image_ready ?
        space->user_image_entry :
        space->user_code_start;
    task->el0.sp = space->user_stack_end & ~((unsigned long)0xF);
    task->el0.spsr = ARM64_SPSR_EL0T;
    task->el0.ready = 1;
}

static struct task tasks[SCHED_MAX_TASKS];
static unsigned char task_stack_region[SCHED_STACK_REGION_SIZE]
    __attribute__((aligned(SCHED_STACK_GUARD_SIZE)));
static struct cpu_context bootstrap_context;
static int current_task;
static int task_count;
static int threads_started;
static unsigned long scheduler_ticks;

static int scheduler_task_is_runnable(const struct task *task)
{
    return task &&
        (task->state == TASK_READY || task->state == TASK_RUNNING);
}

static const char *scheduler_policy_name(const struct task *task)
{
    if (!task)
    {
        return "invalid";
    }

    if (task->state == TASK_ELIGIBLE)
    {
        return "user-eligible";
    }

    if (scheduler_task_is_runnable(task))
    {
        return "runnable";
    }

    if (task->state == TASK_BLOCKED)
    {
        return "blocked";
    }

    return "inactive";
}

static int scheduler_next_runnable_task(void)
{
    int i;
    int candidate;

    for (i = 1; i <= task_count; i++)
    {
        candidate = (current_task + i) % task_count;

        if (candidate != 0 && scheduler_task_is_runnable(&tasks[candidate]))
        {
            return candidate;
        }
    }

    if (scheduler_task_is_runnable(&tasks[0]))
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
    scheduler_clear_memory(&tasks[task_count].memory);
    scheduler_clear_el0(&tasks[task_count].el0);
    scheduler_init_task_memory(&tasks[task_count], pid);
    scheduler_init_task_el0(&tasks[task_count]);

    if (entry)
    {
        stack_top = scheduler_stack_end((unsigned long)task_count);
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

int scheduler_create_blocked_user_task(const char *name)
{
    int pid;

    if (task_count >= SCHED_MAX_TASKS)
    {
        kprintf("Failed to create blocked user task: %s\n", name);
        return -1;
    }

    pid = task_count;
    address_space_init_user(&user_demo_address_space,
                            name ? name : "user-demo",
                            address_space_kernel()->root_table);
    address_space_install_user_smoke_image(&user_demo_address_space);

    tasks[task_count].pid = pid;
    tasks[task_count].name = name ? name : "user-demo";
    tasks[task_count].state = TASK_BLOCKED;
    tasks[task_count].entry = 0;
    scheduler_clear_context(&tasks[task_count].context);
    scheduler_clear_memory(&tasks[task_count].memory);
    scheduler_clear_el0(&tasks[task_count].el0);
    scheduler_init_task_memory(&tasks[task_count], pid);
    tasks[task_count].memory.address_space = &user_demo_address_space;
    scheduler_init_task_el0(&tasks[task_count]);

    task_count++;

    kprintf("Created blocked user task %d: %s\n", pid, tasks[pid].name);
    return pid;
}

int scheduler_unblock_user_task(int pid)
{
    struct address_space *space;
    int status;

    if (pid < 0 || pid >= task_count)
    {
        kprintf("Failed to unblock user task %d: bad pid\n", pid);
        return -1;
    }

    if (tasks[pid].state != TASK_BLOCKED)
    {
        kprintf("Failed to unblock user task %d: state=%s\n",
                pid,
                scheduler_state_name(tasks[pid].state));
        return -1;
    }

    status = user_mode_prepare(&tasks[pid]);
    if (status != USER_MODE_READY)
    {
        kprintf("Failed to unblock user task %d: user_entry=%s\n",
                pid,
                user_mode_status_name(status));
        return -1;
    }

    space = tasks[pid].memory.address_space;
    if (address_space_switch_stub(space) != ADDRESS_SPACE_SWITCH_STUB_READY)
    {
        kprintf("Failed to unblock user task %d: ttbr0_stub=%s\n",
                pid,
                address_space_switch_stub_state(space));
        return -1;
    }

    tasks[pid].state = TASK_ELIGIBLE;
    kprintf("User task %d (%s) is eligible; runnable=no\n",
            pid,
            tasks[pid].name);
    return 0;
}

void scheduler_tick(void)
{
    int previous_task = current_task;

    scheduler_ticks++;

    if (tasks[current_task].state == TASK_RUNNING)
    {
        tasks[current_task].state = TASK_READY;
    }
    current_task = scheduler_next_runnable_task();

    tasks[current_task].state = TASK_RUNNING;

    if (CONFIG_SCHED_DEBUG)
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
        if (scheduler_task_is_runnable(&tasks[previous_task]))
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

    if (CONFIG_SCHED_DEBUG)
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

    if (CONFIG_SCHED_DEBUG)
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
    struct address_space *space;
    int i;

    kprintf("Scheduler state: ticks=%d tasks=%d current=%d\n",
            (int)scheduler_ticks,
            task_count,
            current_task);

    for (i = 0; i < task_count; i++)
    {
        space = tasks[i].memory.address_space;

        kprintf("  task %d: %s state=%s\n",
                tasks[i].pid,
                tasks[i].name,
                scheduler_state_name(tasks[i].state));
        kprintf("    policy=%s runnable=%s\n",
                scheduler_policy_name(&tasks[i]),
                scheduler_task_is_runnable(&tasks[i]) ? "yes" : "no");
        if (space)
        {
            kprintf("    aspace=%s kind=%s root=0x%x shared=%s "
                    "split=%s k_el0=%s u_el0=%s\n",
                    space->name,
                    address_space_kind_name(space->kind),
                    (unsigned int)space->root_table,
                    space->shared_kernel_map ? "yes" : "no",
                    space->permission_split_ready ? "yes" : "no",
                    space->kernel_el0_access ? "yes" : "no",
                    space->user_el0_access ? "yes" : "no");
            kprintf("    user_tables=%s user_desc=%s user_map=%s\n",
                    space->user_tables_ready ? "yes" : "no",
                    space->user_descriptors_ready ? "yes" : "no",
                    space->user_mappings_ready ? "yes" : "no");
            if (space->kind == ADDRESS_SPACE_USER)
            {
                kprintf("    user_image=%s entry=0x%x size=%d checksum=0x%x\n",
                        space->user_image_ready ? "ready" : "missing",
                        (unsigned int)space->user_image_entry,
                        (int)space->user_image_size,
                        (unsigned int)space->user_image_checksum);
            }
            if (space->user_descriptor_count)
            {
                kprintf("    user_desc_count=%d installed=%d\n",
                        (int)space->user_descriptor_count,
                        (int)space->user_installed_descriptor_count);
            }
            kprintf("    aspace_check=%s errors=%d\n",
                    address_space_validation_state(space),
                    (int)space->validation_errors);
            kprintf("    switch=%s active=0x%x target=0x%x ready=%s\n",
                    address_space_switch_state(space),
                    (unsigned int)space->active_root_table,
                    (unsigned int)space->target_root_table,
                    space->switch_ready ? "yes" : "no");
            address_space_switch_stub(space);
            kprintf("    ttbr0_stub=%s\n",
                    address_space_switch_stub_state(space));
        }
        else
        {
            kprintf("    aspace=none kind=none root=0x0 shared=no "
                    "split=no k_el0=no u_el0=no\n");
            kprintf("    user_tables=no user_desc=no user_map=no\n");
            kprintf("    aspace_check=errors errors=1\n");
            kprintf("    switch=blocked active=0x0 target=0x0 ready=no\n");
            kprintf("    ttbr0_stub=blocked\n");
        }
        kprintf("    el0=%s pc=0x%x sp=0x%x spsr=0x%x\n",
                tasks[i].el0.ready ? "yes" : "no",
                (unsigned int)tasks[i].el0.pc,
                (unsigned int)tasks[i].el0.sp,
                (unsigned int)tasks[i].el0.spsr);
        kprintf("    user_entry=%s status=%s\n",
                user_mode_entry_state(&tasks[i]),
                user_mode_status_name(user_mode_prepare(&tasks[i])));
        kprintf("    stack=0x%x-0x%x\n",
                (unsigned int)tasks[i].memory.stack_start,
                (unsigned int)tasks[i].memory.stack_end);
        kprintf("    guards=0x%x-0x%x 0x%x-0x%x\n",
                (unsigned int)tasks[i].memory.guard_low_start,
                (unsigned int)tasks[i].memory.guard_low_end,
                (unsigned int)tasks[i].memory.guard_high_start,
                (unsigned int)tasks[i].memory.guard_high_end);
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

unsigned long scheduler_stack_region_start(void)
{
    return (unsigned long)&task_stack_region[0];
}

unsigned long scheduler_stack_region_end(void)
{
    return scheduler_stack_region_start() + scheduler_stack_region_bytes();
}

unsigned long scheduler_stack_region_bytes(void)
{
    return SCHED_STACK_REGION_SIZE;
}

unsigned long scheduler_stack_count(void)
{
    return SCHED_MAX_TASKS;
}

unsigned long scheduler_stack_guard_start(unsigned long index)
{
    if (index > SCHED_MAX_TASKS)
    {
        index = SCHED_MAX_TASKS;
    }

    return scheduler_stack_region_start() + (index * SCHED_STACK_SLOT_SIZE);
}

unsigned long scheduler_stack_guard_end(unsigned long index)
{
    return scheduler_stack_guard_start(index) + SCHED_STACK_GUARD_SIZE;
}

unsigned long scheduler_stack_start(unsigned long index)
{
    if (index >= SCHED_MAX_TASKS)
    {
        index = SCHED_MAX_TASKS - 1;
    }

    return scheduler_stack_guard_end(index);
}

unsigned long scheduler_stack_end(unsigned long index)
{
    return scheduler_stack_start(index) + SCHED_STACK_SIZE;
}

int scheduler_address_is_stack(unsigned long address)
{
    for (unsigned long i = 0; i < SCHED_MAX_TASKS; i++)
    {
        if (address >= scheduler_stack_start(i) &&
            address < scheduler_stack_end(i))
        {
            return 1;
        }
    }

    return 0;
}

int scheduler_address_is_stack_guard(unsigned long address)
{
    for (unsigned long i = 0; i <= SCHED_MAX_TASKS; i++)
    {
        if (address >= scheduler_stack_guard_start(i) &&
            address < scheduler_stack_guard_end(i))
        {
            return 1;
        }
    }

    return 0;
}
