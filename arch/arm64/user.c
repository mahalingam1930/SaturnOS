#include "user.h"
#include "cpu.h"
#include "kprintf.h"
#include "mmu.h"
#include "scheduler.h"

#define ESR_EC_SHIFT 26
#define ESR_EC_MASK 0x3FUL
#define ESR_ISS_MASK 0x01ffffffUL
#define ESR_EC_BRK 0x3CUL
#define ESR_BRK_IMM0 0x0UL
#define USER_SMOKE_BRK_OFFSET 32UL
#define SPSR_MODE_MASK 0xFUL

enum user_smoke_reject_reason
{
    USER_SMOKE_REJECT_NONE = 0,
    USER_SMOKE_REJECT_BAD_ROOTS,
    USER_SMOKE_REJECT_BAD_RETURN,
    USER_SMOKE_REJECT_UNEXPECTED_EC,
    USER_SMOKE_REJECT_UNEXPECTED_ISS,
    USER_SMOKE_REJECT_UNEXPECTED_MODE,
    USER_SMOKE_REJECT_UNEXPECTED_ELR,
};

struct user_smoke_state
{
    volatile int active;
    volatile int handled;
    volatile int last_reject_reason;
    volatile unsigned long reject_count;
    struct task *task;
    unsigned long expected_elr;
    unsigned long expected_iss;
    unsigned long return_pc;
    unsigned long kernel_root;
    unsigned long user_root;
};

static struct user_smoke_state user_smoke;

static unsigned long exception_class_value(unsigned long esr)
{
    return (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;
}

static unsigned long exception_iss_value(unsigned long esr)
{
    return esr & ESR_ISS_MASK;
}

static const char *user_smoke_reject_reason_name(int reason)
{
    switch (reason)
    {
        case USER_SMOKE_REJECT_NONE:
            return "none";
        case USER_SMOKE_REJECT_BAD_ROOTS:
            return "bad-roots";
        case USER_SMOKE_REJECT_BAD_RETURN:
            return "bad-return";
        case USER_SMOKE_REJECT_UNEXPECTED_EC:
            return "unexpected-ec";
        case USER_SMOKE_REJECT_UNEXPECTED_ISS:
            return "unexpected-iss";
        case USER_SMOKE_REJECT_UNEXPECTED_MODE:
            return "unexpected-mode";
        case USER_SMOKE_REJECT_UNEXPECTED_ELR:
            return "unexpected-elr";
        default:
            return "unknown";
    }
}

static int user_smoke_reject(int reason,
                             unsigned long esr,
                             unsigned long elr,
                             unsigned long spsr)
{
    if (user_smoke.kernel_root)
    {
        arm64_mmu_switch_ttbr0(user_smoke.kernel_root);
    }

    user_smoke.last_reject_reason = reason;
    user_smoke.reject_count++;
    if (user_smoke.task)
    {
        user_smoke.task->user_status.rejects++;
    }
    user_smoke.active = 0;

    kprintf("EL0 smoke: recovery rejected reason=%s ec=0x%x iss=0x%x "
            "elr=0x%x spsr=0x%x\n",
            user_smoke_reject_reason_name(reason),
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

int user_mode_run_smoke_test(struct task *task)
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

    user_smoke.active = 1;
    user_smoke.handled = 0;
    user_smoke.last_reject_reason = USER_SMOKE_REJECT_NONE;
    user_smoke.task = task;
    user_smoke.expected_elr = task->el0.pc + USER_SMOKE_BRK_OFFSET;
    user_smoke.expected_iss = ESR_BRK_IMM0;
    user_smoke.return_pc = (unsigned long)arm64_el0_smoke_return;
    user_smoke.kernel_root = space->kernel_root_table;
    user_smoke.user_root = space->target_root_table;

    kprintf("EL0 smoke: entering user task at 0x%x\n",
            (unsigned int)task->el0.pc);
    kprintf("EL0 smoke: SVC write, exit, then BRK fallback\n");
    kprintf("EL0 smoke: recovery armed ec=0x%x iss=0x%x elr=0x%x\n",
            (unsigned int)ESR_EC_BRK,
            (unsigned int)user_smoke.expected_iss,
            (unsigned int)user_smoke.expected_elr);

    arm64_mmu_switch_ttbr0(user_smoke.user_root);
    task->user_status.el0_entries++;
    arm64_enter_el0_smoke(task->el0.pc, task->el0.sp, task->el0.spsr);

    if (user_smoke.handled)
    {
        task->user_status.recoveries++;
        user_smoke.task = 0;
        kprintf("EL0 smoke: returned to EL1\n");
        return USER_MODE_READY;
    }

    user_smoke.active = 0;
    user_smoke.task = 0;
    arm64_mmu_switch_ttbr0(user_smoke.kernel_root);
    return USER_MODE_SMOKE_FAILED;
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

    if (!user_smoke.active)
    {
        return 0;
    }

    if (!user_smoke.kernel_root || !user_smoke.user_root)
    {
        return user_smoke_reject(USER_SMOKE_REJECT_BAD_ROOTS,
                                 esr,
                                 elr,
                                 spsr);
    }

    if (!user_smoke.return_pc)
    {
        return user_smoke_reject(USER_SMOKE_REJECT_BAD_RETURN,
                                 esr,
                                 elr,
                                 spsr);
    }

    if (ec != ESR_EC_BRK)
    {
        return user_smoke_reject(USER_SMOKE_REJECT_UNEXPECTED_EC,
                                 esr,
                                 elr,
                                 spsr);
    }

    if (iss != user_smoke.expected_iss)
    {
        return user_smoke_reject(USER_SMOKE_REJECT_UNEXPECTED_ISS,
                                 esr,
                                 elr,
                                 spsr);
    }

    if (mode != ARM64_SPSR_MODE_EL0T)
    {
        return user_smoke_reject(USER_SMOKE_REJECT_UNEXPECTED_MODE,
                                 esr,
                                 elr,
                                 spsr);
    }

    if (elr != user_smoke.expected_elr)
    {
        return user_smoke_reject(USER_SMOKE_REJECT_UNEXPECTED_ELR,
                                 esr,
                                 elr,
                                 spsr);
    }

    arm64_mmu_switch_ttbr0(user_smoke.kernel_root);
    if (user_smoke.task)
    {
        user_smoke.task->user_status.expected_traps++;
    }
    user_smoke.handled = 1;
    user_smoke.active = 0;

    kprintf("EL0 smoke: caught expected BRK ec=0x%x iss=0x%x ELR=0x%x\n",
            (unsigned int)ec,
            (unsigned int)iss,
            (unsigned int)elr);

    __asm__ volatile(
        "msr ELR_EL1, %0\n"
        "msr SPSR_EL1, %1\n"
        "isb\n"
        :
        : "r"(user_smoke.return_pc), "r"(ARM64_SPSR_EL1H)
        : "memory");

    return 1;
}

int user_mode_handle_exit_syscall(unsigned long code)
{
    if (!user_smoke.active || !user_smoke.return_pc)
    {
        return 0;
    }

    if (user_smoke.kernel_root)
    {
        arm64_mmu_switch_ttbr0(user_smoke.kernel_root);
    }

    if (user_smoke.task)
    {
        user_smoke.task->user_status.expected_traps++;
        user_smoke.task->user_status.exits++;
        user_smoke.task->user_status.last_exit_code = code;
    }

    user_smoke.handled = 1;
    user_smoke.active = 0;

    kprintf("EL0 smoke: exit syscall code=%d\n", (int)code);

    __asm__ volatile(
        "msr ELR_EL1, %0\n"
        "msr SPSR_EL1, %1\n"
        "isb\n"
        :
        : "r"(user_smoke.return_pc), "r"(ARM64_SPSR_EL1H)
        : "memory");

    return 1;
}

const struct address_space *user_mode_active_address_space(void)
{
    if (user_smoke.active && user_smoke.task)
    {
        return user_smoke.task->memory.address_space;
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
        case USER_MODE_SMOKE_FAILED:
            return "smoke-failed";
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
