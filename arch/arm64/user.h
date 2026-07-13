#ifndef ARM64_USER_H
#define ARM64_USER_H

struct task;

enum user_mode_status
{
    USER_MODE_READY = 0,
    USER_MODE_NO_TASK,
    USER_MODE_NO_ADDRESS_SPACE,
    USER_MODE_NOT_USER_ADDRESS_SPACE,
    USER_MODE_ADDRESS_SPACE_INVALID,
    USER_MODE_EL0_NOT_READY,
    USER_MODE_BAD_ENTRY_STATE,
};

int user_mode_can_enter(const struct task *task);
int user_mode_prepare(const struct task *task);
int user_mode_enter_stub(const struct task *task);
const char *user_mode_status_name(int status);
const char *user_mode_entry_state(const struct task *task);

#endif
