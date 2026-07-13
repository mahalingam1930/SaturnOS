#include "user.h"
#include "cpu.h"
#include "kprintf.h"
#include "mmu.h"
#include "scheduler.h"

#define ESR_EC_SHIFT 26
#define ESR_EC_MASK 0x3FUL
#define ESR_EC_BRK 0x3CUL
#define SPSR_MODE_MASK 0xFUL

struct user_smoke_state
{
    volatile int active;
    volatile int handled;
    unsigned long expected_elr;
    unsigned long return_pc;
    unsigned long kernel_root;
    unsigned long user_root;
};

static struct user_smoke_state user_smoke;

static unsigned long exception_class_value(unsigned long esr)
{
    return (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;
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

int user_mode_run_smoke_test(const struct task *task)
{
    const struct address_space *space;
    int status = user_mode_prepare(task);

    if (status != USER_MODE_READY)
    {
        return status;
    }

    if (task->state != TASK_ELIGIBLE)
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
    user_smoke.expected_elr = task->el0.pc;
    user_smoke.return_pc = (unsigned long)arm64_el0_smoke_return;
    user_smoke.kernel_root = space->kernel_root_table;
    user_smoke.user_root = space->target_root_table;

    kprintf("EL0 smoke: entering user task at 0x%x\n",
            (unsigned int)task->el0.pc);

    arm64_mmu_switch_ttbr0(user_smoke.user_root);
    arm64_enter_el0_smoke(task->el0.pc, task->el0.sp, task->el0.spsr);

    if (user_smoke.handled)
    {
        kprintf("EL0 smoke: returned to EL1\n");
        return USER_MODE_READY;
    }

    user_smoke.active = 0;
    arm64_mmu_switch_ttbr0(user_smoke.kernel_root);
    return USER_MODE_SMOKE_FAILED;
}

int user_mode_handle_exception(unsigned long esr,
                               unsigned long elr,
                               unsigned long far,
                               unsigned long spsr)
{
    unsigned long ec = exception_class_value(esr);
    unsigned long mode = spsr & SPSR_MODE_MASK;
    (void)far;

    if (!user_smoke.active)
    {
        return 0;
    }

    if (ec != ESR_EC_BRK ||
        mode != ARM64_SPSR_MODE_EL0T ||
        elr != user_smoke.expected_elr)
    {
        arm64_mmu_switch_ttbr0(user_smoke.kernel_root);
        user_smoke.active = 0;
        return 0;
    }

    arm64_mmu_switch_ttbr0(user_smoke.kernel_root);
    user_smoke.handled = 1;
    user_smoke.active = 0;

    kprintf("EL0 smoke: caught expected BRK at ELR=0x%x\n",
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
