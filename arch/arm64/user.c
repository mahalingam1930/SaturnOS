#include "user.h"
#include "cpu.h"
#include "kprintf.h"
#include "mmu.h"
#include "scheduler.h"

#define ESR_EC_SHIFT 26
#define ESR_EC_MASK 0x3FUL
#define ESR_ISS_MASK 0x01ffffffUL
#define SPSR_MODE_MASK 0xFUL

enum user_session_reject_reason
{
    USER_SESSION_REJECT_NONE = 0,
    USER_SESSION_REJECT_BAD_ROOTS,
    USER_SESSION_REJECT_BAD_RETURN,
    USER_SESSION_REJECT_UNEXPECTED_MODE,
};

struct user_session_state
{
    volatile int active;
    volatile int completed;
    volatile int faulted;
    volatile int last_reject_reason;
    volatile unsigned long reject_count;
    struct task *task;
    unsigned long return_pc;
    unsigned long kernel_root;
    unsigned long user_root;
};

static struct user_session_state user_session;

static unsigned long exception_class_value(unsigned long esr)
{
    return (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;
}

static unsigned long exception_iss_value(unsigned long esr)
{
    return esr & ESR_ISS_MASK;
}

static const char *user_session_reject_reason_name(int reason)
{
    switch (reason)
    {
        case USER_SESSION_REJECT_NONE:
            return "none";
        case USER_SESSION_REJECT_BAD_ROOTS:
            return "bad-roots";
        case USER_SESSION_REJECT_BAD_RETURN:
            return "bad-return";
        case USER_SESSION_REJECT_UNEXPECTED_MODE:
            return "unexpected-mode";
        default:
            return "unknown";
    }
}

static int user_session_reject(int reason,
                             unsigned long esr,
                             unsigned long elr,
                             unsigned long spsr)
{
    if (user_session.kernel_root)
    {
        arm64_mmu_switch_ttbr0(user_session.kernel_root);
    }

    user_session.last_reject_reason = reason;
    user_session.reject_count++;
    if (user_session.task)
    {
        user_session.task->user_status.rejects++;
    }
    user_session.active = 0;

    kprintf("EL0 task: recovery rejected reason=%s ec=0x%x iss=0x%x "
            "elr=0x%x spsr=0x%x\n",
            user_session_reject_reason_name(reason),
            (unsigned int)exception_class_value(esr),
            (unsigned int)exception_iss_value(esr),
            (unsigned int)elr,
            (unsigned int)spsr);

    return 0;
}

int user_mode_can_enter(const struct task *task)
{
    const struct address_space *space;

    if (!task)
    {
        return USER_MODE_NO_TASK;
    }

    space = task->memory.address_space;
    if (!space)
    {
        return USER_MODE_NO_ADDRESS_SPACE;
    }

    if (space->kind != ADDRESS_SPACE_USER)
    {
        return USER_MODE_NOT_USER_ADDRESS_SPACE;
    }

    if (!space->validation_ready || space->validation_errors)
    {
        return USER_MODE_ADDRESS_SPACE_INVALID;
    }

    if (!space->user_image_ready)
    {
        return USER_MODE_NO_IMAGE;
    }

    if (!task->el0.ready)
    {
        return USER_MODE_EL0_NOT_READY;
    }

    if (!task->el0.pc ||
        !task->el0.sp ||
        (task->el0.sp & 0xFUL) != 0 ||
        task->el0.spsr != ARM64_SPSR_EL0T)
    {
        return USER_MODE_BAD_ENTRY_STATE;
    }

    return USER_MODE_READY;
}

int user_mode_prepare(const struct task *task)
{
    return user_mode_can_enter(task);
}

int user_mode_enter_stub(const struct task *task)
{
    int status = user_mode_prepare(task);

    if (status != USER_MODE_READY)
    {
        return status;
    }

    /*
     * The real EL0 return path exists now, but normal boot never reaches this
     * call because no current kernel task is EL0-ready.
     */
    arm64_enter_el0(task->el0.pc, task->el0.sp, task->el0.spsr);

    return USER_MODE_READY;
}

int user_mode_run_task(struct task *task)
{
    const struct address_space *space;
    int status = user_mode_prepare(task);

    if (status != USER_MODE_READY)
    {
        return status;
    }

    if (task->state != TASK_ELIGIBLE && task->state != TASK_RUNNING)
    {
        return USER_MODE_NOT_ELIGIBLE;
    }

    space = task->memory.address_space;
    if (!space ||
        address_space_switch_stub((struct address_space *)space) !=
            ADDRESS_SPACE_SWITCH_STUB_READY)
    {
        return USER_MODE_SWITCH_NOT_READY;
    }

    user_session.active = 1;
    user_session.completed = 0;
    user_session.faulted = 0;
    user_session.last_reject_reason = USER_SESSION_REJECT_NONE;
    user_session.task = task;
    user_session.return_pc = (unsigned long)arm64_el0_task_return;
    user_session.kernel_root = space->kernel_root_table;
    user_session.user_root = space->target_root_table;

    kprintf("EL0 task: entering at 0x%x\n",
            (unsigned int)task->el0.pc);
    kprintf("EL0 task: completion via exit syscall; faults recover to EL1\n");

    arm64_mmu_switch_ttbr0(user_session.user_root);
    task->user_status.el0_entries++;
    arm64_enter_el0_task(task->el0.pc,
                        task->el0.sp,
                        task->el0.spsr,
                        task->user_argc,
                        task->user_argv_address,
                        task->user_arg_lengths_address);

    if (user_session.completed)
    {
        task->user_status.recoveries++;
        user_session.task = 0;
        kprintf("EL0 task: returned to EL1 outcome=%s\n",
                user_session.faulted ? "fault" : "exit");
        return user_session.faulted ? USER_MODE_TASK_FAILED : USER_MODE_READY;
    }

    user_session.active = 0;
    user_session.task = 0;
    arm64_mmu_switch_ttbr0(user_session.kernel_root);
    return USER_MODE_TASK_FAILED;
}

int user_mode_handle_exception(unsigned long esr,
                               unsigned long elr,
                               unsigned long far,
                               unsigned long spsr)
{
    unsigned long ec = exception_class_value(esr);
    unsigned long iss = exception_iss_value(esr);
    unsigned long mode = spsr & SPSR_MODE_MASK;
    (void)far;

    if (!user_session.active)
    {
        return 0;
    }

    if (!user_session.kernel_root || !user_session.user_root)
    {
        return user_session_reject(USER_SESSION_REJECT_BAD_ROOTS,
                                 esr,
                                 elr,
                                 spsr);
    }

    if (!user_session.return_pc)
    {
        return user_session_reject(USER_SESSION_REJECT_BAD_RETURN,
                                 esr,
                                 elr,
                                 spsr);
    }

    if (mode != ARM64_SPSR_MODE_EL0T)
    {
        return user_session_reject(USER_SESSION_REJECT_UNEXPECTED_MODE,
                                 esr,
                                 elr,
                                 spsr);
    }

    arm64_mmu_switch_ttbr0(user_session.kernel_root);
    if (user_session.task)
    {
        user_session.task->user_status.traps++;
        user_session.task->user_status.rejects++;
    }
    user_session.reject_count++;
    user_session.last_reject_reason = USER_SESSION_REJECT_NONE;
    user_session.completed = 1;
    user_session.faulted = 1;
    user_session.active = 0;

    kprintf("EL0 task: fault recovered ec=0x%x iss=0x%x elr=0x%x far=0x%x\n",
            (unsigned int)ec,
            (unsigned int)iss,
            (unsigned int)elr,
            (unsigned int)far);

    __asm__ volatile(
        "msr ELR_EL1, %0\n"
        "msr SPSR_EL1, %1\n"
        "isb\n"
        :
        : "r"(user_session.return_pc), "r"(ARM64_SPSR_EL1H)
        : "memory");

    return 1;
}

int user_mode_handle_exit_syscall(unsigned long code)
{
    if (!user_session.active || !user_session.return_pc)
    {
        return 0;
    }

    if (user_session.kernel_root)
    {
        arm64_mmu_switch_ttbr0(user_session.kernel_root);
    }

    if (user_session.task)
    {
        user_session.task->user_status.traps++;
        user_session.task->user_status.exits++;
        user_session.task->user_status.last_exit_code = code;
    }

    user_session.completed = 1;
    user_session.faulted = 0;
    user_session.active = 0;

    kprintf("EL0 task: exit code=%d\n", (int)code);

    __asm__ volatile(
        "msr ELR_EL1, %0\n"
        "msr SPSR_EL1, %1\n"
        "isb\n"
        :
        : "r"(user_session.return_pc), "r"(ARM64_SPSR_EL1H)
        : "memory");

    return 1;
}

const struct address_space *user_mode_active_address_space(void)
{
    if (user_session.active && user_session.task)
    {
        return user_session.task->memory.address_space;
    }

    return 0;
}

const char *user_mode_status_name(int status)
{
    switch (status)
    {
        case USER_MODE_READY:
            return "ready";
        case USER_MODE_NO_TASK:
            return "no-task";
        case USER_MODE_NO_ADDRESS_SPACE:
            return "no-aspace";
        case USER_MODE_NOT_USER_ADDRESS_SPACE:
            return "kernel-task";
        case USER_MODE_ADDRESS_SPACE_INVALID:
            return "bad-aspace";
        case USER_MODE_NO_IMAGE:
            return "no-image";
        case USER_MODE_EL0_NOT_READY:
            return "no-el0";
        case USER_MODE_BAD_ENTRY_STATE:
            return "bad-entry";
        case USER_MODE_NOT_ELIGIBLE:
            return "not-eligible";
        case USER_MODE_SWITCH_NOT_READY:
            return "switch-not-ready";
        case USER_MODE_TASK_FAILED:
            return "task-failed";
        default:
            return "unknown";
    }
}

const char *user_mode_entry_state(const struct task *task)
{
    if (user_mode_can_enter(task) == USER_MODE_READY)
    {
        return "ready";
    }

    return "blocked";
}
