#include "user.h"
#include "cpu.h"
#include "scheduler.h"

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
