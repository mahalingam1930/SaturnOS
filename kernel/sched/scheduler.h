#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "context.h"
#include "address_space.h"

#define SCHED_MAX_TASKS 8
#define SCHED_STACK_SIZE 4096
#define SCHED_STACK_GUARD_SIZE 4096
#define SCHED_STACK_SLOT_SIZE (SCHED_STACK_GUARD_SIZE + SCHED_STACK_SIZE)
#define SCHED_STACK_REGION_SIZE \
    ((SCHED_MAX_TASKS * SCHED_STACK_SLOT_SIZE) + SCHED_STACK_GUARD_SIZE)

enum task_state
{
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_ELIGIBLE,
    TASK_ZOMBIE
};

struct task_memory
{
    struct address_space *address_space;
    unsigned long stack_start;
    unsigned long stack_end;
    unsigned long guard_low_start;
    unsigned long guard_low_end;
    unsigned long guard_high_start;
    unsigned long guard_high_end;
};

struct task_el0_state
{
    unsigned long pc;
    unsigned long sp;
    unsigned long spsr;
    int ready;
};

struct task_user_status
{
    int smoke_completed;
    int smoke_passed;
    unsigned long admissions;
    unsigned long el0_entries;
    unsigned long expected_traps;
    unsigned long recoveries;
    unsigned long rejects;
    unsigned long completions;
    unsigned long failures;
};

struct task_accounting
{
    unsigned long switches;
    unsigned long run_ticks;
    unsigned long yields;
    unsigned long preemptions;
    unsigned long sleep_wakeups;
};

struct task
{
    int pid;
    const char *name;
    enum task_state state;
    void (*entry)(void);
    struct cpu_context context;
    unsigned long sleep_until_tick;
    unsigned long sleep_requested_ms;
    struct task_accounting accounting;
    struct task_memory memory;
    struct task_el0_state el0;
    struct task_user_status user_status;
};

void scheduler_init(void);
int scheduler_create_kernel_thread(const char *name, void (*entry)(void));
int scheduler_create_blocked_user_task(const char *name);
int scheduler_unblock_user_task(int pid);
int scheduler_block_task(int pid);
int scheduler_unblock_task(int pid);
int scheduler_reap_zombie_task(int pid);
int scheduler_reap_zombies(void);
int scheduler_run_user_smoke_test(int pid);
void scheduler_tick(void);
void scheduler_yield(void);
void scheduler_sleep_ms(unsigned long ms);
void scheduler_preempt(void);
void scheduler_exit(void);
void scheduler_start_threads(void);
void scheduler_dump_tasks(void);
void scheduler_dump_task_summary(void);
int scheduler_dump_task_status(int pid);
void scheduler_dump_user_stats(void);
const struct task *scheduler_current_task(void);
unsigned long scheduler_get_ticks(void);
unsigned long scheduler_stack_region_start(void);
unsigned long scheduler_stack_region_end(void);
unsigned long scheduler_stack_region_bytes(void);
unsigned long scheduler_stack_count(void);
unsigned long scheduler_stack_start(unsigned long index);
unsigned long scheduler_stack_end(unsigned long index);
unsigned long scheduler_stack_guard_start(unsigned long index);
unsigned long scheduler_stack_guard_end(unsigned long index);
int scheduler_address_is_stack(unsigned long address);
int scheduler_address_is_stack_guard(unsigned long address);

#endif
