#include "scheduler.h"
#include "config.h"
#include "cpu.h"
#include "irq.h"
#include "kprintf.h"
#include "timer.h"
#include "user.h"
#include "programs.h"
#include "executable.h"
#include "vfs.h"

static void idle_task(void);
static void scheduler_exit_task(int task_id);
static void scheduler_user_task_entry(void);
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
        case TASK_BLOCKED:
            return "blocked";
        case TASK_SLEEPING:
            return "sleeping";
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

static void scheduler_clear_user_status(struct task_user_status *status)
{
    status->program_completed = 0;
    status->program_succeeded = 0;
    status->admissions = 0;
    status->el0_entries = 0;
    status->traps = 0;
    status->recoveries = 0;
    status->rejects = 0;
    status->exits = 0;
    status->last_exit_code = 0;
    status->completions = 0;
    status->failures = 0;
}

static void scheduler_clear_accounting(struct task_accounting *accounting)
{
    accounting->switches = 0;
    accounting->run_ticks = 0;
    accounting->yields = 0;
    accounting->preemptions = 0;
    accounting->sleep_wakeups = 0;
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

static void scheduler_clear_task_slot(int pid)
{
    address_space_destroy_user(tasks[pid].memory.address_space);
    tasks[pid].pid = pid;
    tasks[pid].name = "unused";
    tasks[pid].state = TASK_UNUSED;
    tasks[pid].entry = 0;
    tasks[pid].sleep_until_tick = 0;
    tasks[pid].sleep_requested_ms = 0;
    scheduler_clear_context(&tasks[pid].context);
    scheduler_clear_accounting(&tasks[pid].accounting);
    scheduler_clear_memory(&tasks[pid].memory);
    scheduler_clear_el0(&tasks[pid].el0);
    scheduler_clear_user_status(&tasks[pid].user_status);
}

static void scheduler_trim_unused_tail(void)
{
    while (task_count > 1 && tasks[task_count - 1].state == TASK_UNUSED)
    {
        task_count--;
    }
}

static int scheduler_task_is_runnable(const struct task *task)
{
    return task &&
        (task->state == TASK_READY || task->state == TASK_RUNNING);
}

static void scheduler_set_running_task(int pid)
{
    tasks[pid].state = TASK_RUNNING;
    tasks[pid].accounting.switches++;
}

static int scheduler_task_is_kernel_task(const struct task *task)
{
    return task &&
        task->memory.address_space &&
        task->memory.address_space->kind == ADDRESS_SPACE_KERNEL;
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

    if (task->state == TASK_SLEEPING)
    {
        return "sleeping";
    }

    return "inactive";
}

static void scheduler_wake_sleeping_tasks(void)
{
    for (int i = 0; i < task_count; i++)
    {
        if (tasks[i].state == TASK_SLEEPING &&
            scheduler_ticks >= tasks[i].sleep_until_tick)
        {
            tasks[i].state = TASK_READY;
            tasks[i].sleep_until_tick = 0;
            tasks[i].sleep_requested_ms = 0;
            tasks[i].accounting.sleep_wakeups++;
        }
    }
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

    pid = -1;

    for (int i = 1; i < task_count; i++)
    {
        if (tasks[i].state == TASK_UNUSED)
        {
            pid = i;
            break;
        }
    }

    if (pid < 0)
    {
        if (task_count >= SCHED_MAX_TASKS)
        {
            return -1;
        }

        pid = task_count;
        task_count++;
    }

    tasks[pid].pid = pid;
    tasks[pid].name = name;
    tasks[pid].state = TASK_READY;
    tasks[pid].entry = entry;
    tasks[pid].sleep_until_tick = 0;
    tasks[pid].sleep_requested_ms = 0;
    scheduler_clear_accounting(&tasks[pid].accounting);
    scheduler_clear_context(&tasks[pid].context);
    scheduler_clear_memory(&tasks[pid].memory);
    scheduler_clear_el0(&tasks[pid].el0);
    scheduler_clear_user_status(&tasks[pid].user_status);
    scheduler_init_task_memory(&tasks[pid], pid);
    scheduler_init_task_el0(&tasks[pid]);

    if (entry)
    {
        stack_top = scheduler_stack_end((unsigned long)pid);
        stack_top &= ~((unsigned long)0xF);

        tasks[pid].context.sp = stack_top;

        if (pid == 0)
        {
            tasks[pid].context.lr = (unsigned long)entry;
        }
        else
        {
            tasks[pid].context.lr = (unsigned long)thread_trampoline;
        }
    }

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
    return scheduler_create_user_task_from_image(name, USER_DEMO_IMAGE_PATH);
}

int scheduler_create_user_task_from_image(const char *name,
                                          const char *image_path)
{
    const void *file;
    unsigned long file_size;
    struct saturn_exec_image image;
    int pid;

    file = vfs_file_data(image_path, &file_size);
    if (!file)
    {
        kprintf("Failed to create user task: image file missing\n");
        return -1;
    }
    if (!saturn_exec_parse(file, file_size, &image))
    {
        kprintf("Failed to create user task: invalid executable\n");
        return -1;
    }

    pid = -1;

    for (int i = 1; i < task_count; i++)
    {
        if (tasks[i].state == TASK_UNUSED)
        {
            pid = i;
            break;
        }
    }

    if (pid < 0)
    {
        if (task_count >= SCHED_MAX_TASKS)
        {
            kprintf("Failed to create blocked user task: %s\n", name);
            return -1;
        }

        pid = task_count;
        task_count++;
    }

    if (pid >= SCHED_MAX_TASKS)
    {
        kprintf("Failed to create blocked user task: %s\n", name);
        return -1;
    }

    address_space_init_user(&tasks[pid].user_address_space,
                            name ? name : "user-demo",
                            address_space_kernel()->root_table);
    if (!address_space_load_user_image(&tasks[pid].user_address_space,
                                       image.code,
                                       image.code_size,
                                       image.data,
                                       image.data_size,
                                       image.entry_offset))
    {
        kprintf("Failed to create user task: invalid image\n");
        address_space_destroy_user(&tasks[pid].user_address_space);
        if (pid == task_count - 1)
        {
            task_count--;
        }
        return -1;
    }

    tasks[pid].pid = pid;
    tasks[pid].name = name ? name : "user-demo";
    tasks[pid].state = TASK_BLOCKED;
    tasks[pid].entry = 0;
    tasks[pid].sleep_until_tick = 0;
    tasks[pid].sleep_requested_ms = 0;
    scheduler_clear_accounting(&tasks[pid].accounting);
    scheduler_clear_context(&tasks[pid].context);
    scheduler_clear_memory(&tasks[pid].memory);
    scheduler_clear_el0(&tasks[pid].el0);
    scheduler_clear_user_status(&tasks[pid].user_status);
    scheduler_init_task_memory(&tasks[pid], pid);
    tasks[pid].memory.address_space = &tasks[pid].user_address_space;
    scheduler_init_task_el0(&tasks[pid]);

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

    tasks[pid].entry = scheduler_user_task_entry;
    tasks[pid].context.sp = scheduler_stack_end((unsigned long)pid);
    tasks[pid].context.sp &= ~((unsigned long)0xF);
    tasks[pid].context.lr = (unsigned long)thread_trampoline;
    tasks[pid].state = TASK_READY;
    tasks[pid].user_status.admissions++;
    kprintf("User task %d (%s) is ready; runnable=yes\n",
            pid,
            tasks[pid].name);
    return 0;
}

int scheduler_block_task(int pid)
{
    if (pid < 0 || pid >= task_count)
    {
        kprintf("Failed to block task %d: bad pid\n", pid);
        return -1;
    }

    if (pid == 0)
    {
        kprintf("Failed to block task %d: idle task is protected\n", pid);
        return -1;
    }

    if (pid == current_task)
    {
        kprintf("Failed to block task %d: current task is protected\n", pid);
        return -1;
    }

    if (!scheduler_task_is_kernel_task(&tasks[pid]))
    {
        kprintf("Failed to block task %d: only kernel tasks are supported\n",
                pid);
        return -1;
    }

    if (tasks[pid].state != TASK_READY)
    {
        kprintf("Failed to block task %d: state=%s\n",
                pid,
                scheduler_state_name(tasks[pid].state));
        return -1;
    }

    tasks[pid].state = TASK_BLOCKED;
    tasks[pid].sleep_until_tick = 0;
    tasks[pid].sleep_requested_ms = 0;

    kprintf("Blocked task %d (%s)\n", pid, tasks[pid].name);
    return 0;
}

int scheduler_unblock_task(int pid)
{
    if (pid < 0 || pid >= task_count)
    {
        kprintf("Failed to unblock task %d: bad pid\n", pid);
        return -1;
    }

    if (pid == 0)
    {
        kprintf("Failed to unblock task %d: idle task is protected\n", pid);
        return -1;
    }

    if (!scheduler_task_is_kernel_task(&tasks[pid]))
    {
        kprintf("Failed to unblock task %d: only kernel tasks are supported\n",
                pid);
        return -1;
    }

    if (tasks[pid].state != TASK_BLOCKED)
    {
        kprintf("Failed to unblock task %d: state=%s\n",
                pid,
                scheduler_state_name(tasks[pid].state));
        return -1;
    }

    tasks[pid].state = TASK_READY;

    kprintf("Unblocked task %d (%s)\n", pid, tasks[pid].name);
    return 0;
}

int scheduler_reap_zombie_task(int pid)
{
    if (pid < 0 || pid >= task_count)
    {
        kprintf("Failed to reap task %d: bad pid\n", pid);
        return -1;
    }

    if (pid == 0)
    {
        kprintf("Failed to reap task %d: idle task is protected\n", pid);
        return -1;
    }

    if (pid == current_task)
    {
        kprintf("Failed to reap task %d: current task is protected\n", pid);
        return -1;
    }

    if (tasks[pid].state != TASK_ZOMBIE)
    {
        kprintf("Failed to reap task %d: state=%s\n",
                pid,
                scheduler_state_name(tasks[pid].state));
        return -1;
    }

    kprintf("Reaped task %d (%s)\n", pid, tasks[pid].name);
    scheduler_clear_task_slot(pid);
    scheduler_trim_unused_tail();
    return 0;
}

int scheduler_reap_zombies(void)
{
    int reaped = 0;

    for (int i = 1; i < task_count; i++)
    {
        if (i != current_task && tasks[i].state == TASK_ZOMBIE)
        {
            kprintf("Reaped task %d (%s)\n", i, tasks[i].name);
            scheduler_clear_task_slot(i);
            reaped++;
        }
    }

    scheduler_trim_unused_tail();

    if (!reaped)
    {
        kprintf("No zombie tasks to reap\n");
    }

    return reaped;
}

int scheduler_run_user_task(int pid)
{
    int status;

    if (pid < 0 || pid >= task_count)
    {
        kprintf("Failed to run user task %d: bad pid\n", pid);
        return -1;
    }

    if (tasks[pid].state != TASK_ELIGIBLE &&
        tasks[pid].state != TASK_RUNNING)
    {
        kprintf("Failed to run user task %d: state=%s\n",
                pid,
                scheduler_state_name(tasks[pid].state));
        return -1;
    }

    kprintf("Starting EL0 program for task %d (%s)\n",
            pid,
            tasks[pid].name);

    status = user_mode_run_task(&tasks[pid]);
    if (status != USER_MODE_READY)
    {
        tasks[pid].user_status.program_completed = 1;
        tasks[pid].user_status.program_succeeded = 0;
        tasks[pid].user_status.failures++;
        tasks[pid].state = TASK_ZOMBIE;
        kprintf("User task %d (%s) program=failed status=%s\n",
                pid,
                tasks[pid].name,
                user_mode_status_name(status));
        return -1;
    }

    tasks[pid].user_status.program_completed = 1;
    tasks[pid].user_status.program_succeeded = 1;
    tasks[pid].user_status.completions++;
    tasks[pid].state = TASK_ZOMBIE;

    kprintf("User task %d (%s) program=completed exit=%d\n",
            pid,
            tasks[pid].name,
            (int)tasks[pid].user_status.last_exit_code);
    kprintf("User task %d (%s) completed; state=%s runnable=no\n",
            pid,
            tasks[pid].name,
            scheduler_state_name(tasks[pid].state));
    return 0;
}

static void scheduler_user_task_entry(void)
{
    int pid = current_task;

    kprintf("Scheduled user task entry: task %d\n", pid);
    scheduler_run_user_task(pid);
    kprintf("Scheduled user task entry: done\n");
}

void scheduler_tick(void)
{
    int previous_task = current_task;

    scheduler_ticks++;
    scheduler_wake_sleeping_tasks();
    tasks[previous_task].accounting.run_ticks++;

    if (tasks[current_task].state == TASK_RUNNING)
    {
        tasks[current_task].state = TASK_READY;
    }
    current_task = scheduler_next_runnable_task();

    tasks[previous_task].accounting.preemptions++;
    scheduler_set_running_task(current_task);

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
    scheduler_wake_sleeping_tasks();
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
        tasks[previous_task].accounting.yields++;
    }

    scheduler_set_running_task(next_task);
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

void scheduler_sleep_ms(unsigned long ms)
{
    unsigned long interval_ms = timer_get_interval_ms();
    unsigned long sleep_ticks;

    if (!threads_started || current_task == 0 || !interval_ms)
    {
        timer_sleep_ms(ms);
        return;
    }

    sleep_ticks = (ms + interval_ms - 1) / interval_ms;
    if (sleep_ticks == 0)
    {
        sleep_ticks = 1;
    }

    tasks[current_task].sleep_requested_ms = ms;
    tasks[current_task].sleep_until_tick = scheduler_ticks + sleep_ticks;
    tasks[current_task].state = TASK_SLEEPING;

    scheduler_yield();
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
    scheduler_wake_sleeping_tasks();

    previous_task = current_task;
    tasks[previous_task].accounting.run_ticks++;
    next_task = scheduler_next_runnable_task();

    if (next_task == previous_task)
    {
        return;
    }

    if (tasks[previous_task].state == TASK_RUNNING)
    {
        tasks[previous_task].state = TASK_READY;
    }
    tasks[previous_task].accounting.preemptions++;

    scheduler_set_running_task(next_task);
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
    scheduler_set_running_task(current_task);

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
        kprintf("    account switches=%d run_ticks=%d yields=%d "
                "preempt=%d wakeups=%d\n",
                (int)tasks[i].accounting.switches,
                (int)tasks[i].accounting.run_ticks,
                (int)tasks[i].accounting.yields,
                (int)tasks[i].accounting.preemptions,
                (int)tasks[i].accounting.sleep_wakeups);
        if (tasks[i].state == TASK_SLEEPING)
        {
            kprintf("    sleep requested=%dms until_tick=%d now=%d\n",
                    (int)tasks[i].sleep_requested_ms,
                    (int)tasks[i].sleep_until_tick,
                    (int)scheduler_ticks);
        }
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
                kprintf("    user_program=%s result=%s\n",
                        tasks[i].user_status.program_completed ?
                            "completed" : "pending",
                        tasks[i].user_status.program_completed ?
                            (tasks[i].user_status.program_succeeded ?
                                "passed" : "failed") :
                            "none");
                kprintf("    user_counts admit=%d enter=%d trap=%d "
                        "recover=%d reject=%d exit=%d code=%d "
                        "complete=%d fail=%d\n",
                        (int)tasks[i].user_status.admissions,
                        (int)tasks[i].user_status.el0_entries,
                        (int)tasks[i].user_status.traps,
                        (int)tasks[i].user_status.recoveries,
                        (int)tasks[i].user_status.rejects,
                        (int)tasks[i].user_status.exits,
                        (int)tasks[i].user_status.last_exit_code,
                        (int)tasks[i].user_status.completions,
                        (int)tasks[i].user_status.failures);
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

void scheduler_dump_task_summary(void)
{
    const struct address_space *space;

    kprintf("Task summary: ticks=%d tasks=%d current=%d\n",
            (int)scheduler_ticks,
            task_count,
            current_task);

    for (int i = 0; i < task_count; i++)
    {
        space = tasks[i].memory.address_space;
        kprintf("  pid=%d name=%s state=%s run=%s kind=%s sw=%d ticks=%d\n",
                tasks[i].pid,
                tasks[i].name,
                scheduler_state_name(tasks[i].state),
                scheduler_task_is_runnable(&tasks[i]) ? "yes" : "no",
                space ? address_space_kind_name(space->kind) : "none",
                (int)tasks[i].accounting.switches,
                (int)tasks[i].accounting.run_ticks);
    }
}

int scheduler_dump_task_status(int pid)
{
    const struct address_space *space;
    const struct task_user_status *status;

    if (pid < 0 || pid >= task_count)
    {
        kprintf("Task %d not found\n", pid);
        return -1;
    }

    space = tasks[pid].memory.address_space;
    kprintf("Task %d:\n", tasks[pid].pid);
    kprintf("  name=%s state=%s policy=%s runnable=%s current=%s\n",
            tasks[pid].name,
            scheduler_state_name(tasks[pid].state),
            scheduler_policy_name(&tasks[pid]),
            scheduler_task_is_runnable(&tasks[pid]) ? "yes" : "no",
            pid == current_task ? "yes" : "no");
    if (tasks[pid].state == TASK_SLEEPING)
    {
        kprintf("  sleep requested=%dms until_tick=%d now=%d\n",
                (int)tasks[pid].sleep_requested_ms,
                (int)tasks[pid].sleep_until_tick,
                (int)scheduler_ticks);
    }
    kprintf("  account switches=%d run_ticks=%d yields=%d preempt=%d "
            "wakeups=%d\n",
            (int)tasks[pid].accounting.switches,
            (int)tasks[pid].accounting.run_ticks,
            (int)tasks[pid].accounting.yields,
            (int)tasks[pid].accounting.preemptions,
            (int)tasks[pid].accounting.sleep_wakeups);

    if (space)
    {
        kprintf("  aspace=%s kind=%s root=0x%x target=0x%x\n",
                space->name,
                address_space_kind_name(space->kind),
                (unsigned int)space->root_table,
                (unsigned int)space->target_root_table);
        kprintf("  switch=%s check=%s errors=%d\n",
                address_space_switch_state(space),
                address_space_validation_state(space),
                (int)space->validation_errors);
    }
    else
    {
        kprintf("  aspace=none kind=none root=0x0 target=0x0\n");
        kprintf("  switch=blocked check=errors errors=1\n");
    }

    kprintf("  stack=0x%x-0x%x\n",
            (unsigned int)tasks[pid].memory.stack_start,
            (unsigned int)tasks[pid].memory.stack_end);
    kprintf("  guards=0x%x-0x%x 0x%x-0x%x\n",
            (unsigned int)tasks[pid].memory.guard_low_start,
            (unsigned int)tasks[pid].memory.guard_low_end,
            (unsigned int)tasks[pid].memory.guard_high_start,
            (unsigned int)tasks[pid].memory.guard_high_end);
    kprintf("  el0=%s pc=0x%x sp=0x%x spsr=0x%x\n",
            tasks[pid].el0.ready ? "yes" : "no",
            (unsigned int)tasks[pid].el0.pc,
            (unsigned int)tasks[pid].el0.sp,
            (unsigned int)tasks[pid].el0.spsr);
    kprintf("  user_entry=%s status=%s\n",
            user_mode_entry_state(&tasks[pid]),
            user_mode_status_name(user_mode_prepare(&tasks[pid])));

    if (space && space->kind == ADDRESS_SPACE_USER)
    {
        status = &tasks[pid].user_status;
        kprintf("  user_program=%s result=%s\n",
                status->program_completed ? "completed" : "pending",
                status->program_completed ?
                    (status->program_succeeded ? "passed" : "failed") :
                    "none");
        kprintf("  user_counts admit=%d enter=%d trap=%d recover=%d\n",
                (int)status->admissions,
                (int)status->el0_entries,
                (int)status->traps,
                (int)status->recoveries);
        kprintf("  user_counts reject=%d exit=%d code=%d complete=%d "
                "fail=%d\n",
                (int)status->rejects,
                (int)status->exits,
                (int)status->last_exit_code,
                (int)status->completions,
                (int)status->failures);
    }

    return 0;
}

void scheduler_dump_user_stats(void)
{
    const struct task_user_status *status;
    const struct address_space *space;
    int user_tasks = 0;

    kprintf("User exception stats:\n");

    for (int i = 0; i < task_count; i++)
    {
        space = tasks[i].memory.address_space;
        if (!space || space->kind != ADDRESS_SPACE_USER)
        {
            continue;
        }

        user_tasks++;
        status = &tasks[i].user_status;
        kprintf("  task %d: %s state=%s\n",
                tasks[i].pid,
                tasks[i].name,
                scheduler_state_name(tasks[i].state));
        kprintf("    program=%s result=%s\n",
                status->program_completed ? "completed" : "pending",
                status->program_completed ?
                    (status->program_succeeded ? "passed" : "failed") :
                    "none");
        kprintf("    admit=%d enter=%d trap=%d recover=%d\n",
                (int)status->admissions,
                (int)status->el0_entries,
                (int)status->traps,
                (int)status->recoveries);
        kprintf("    reject=%d exit=%d code=%d complete=%d fail=%d\n",
                (int)status->rejects,
                (int)status->exits,
                (int)status->last_exit_code,
                (int)status->completions,
                (int)status->failures);
        kprintf("    entry=%s status=%s\n",
                user_mode_entry_state(&tasks[i]),
                user_mode_status_name(user_mode_prepare(&tasks[i])));
    }

    if (!user_tasks)
    {
        kprintf("  no user tasks\n");
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
