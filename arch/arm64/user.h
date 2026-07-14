#ifndef ARM64_USER_H
#define ARM64_USER_H

struct task;
struct address_space;

enum user_mode_status
{
    USER_MODE_READY = 0,
    USER_MODE_NO_TASK,
    USER_MODE_NO_ADDRESS_SPACE,
    USER_MODE_NOT_USER_ADDRESS_SPACE,
    USER_MODE_ADDRESS_SPACE_INVALID,
    USER_MODE_NO_IMAGE,
    USER_MODE_EL0_NOT_READY,
    USER_MODE_BAD_ENTRY_STATE,
    USER_MODE_NOT_ELIGIBLE,
    USER_MODE_SWITCH_NOT_READY,
    USER_MODE_SMOKE_FAILED,
};

int user_mode_can_enter(const struct task *task);
int user_mode_prepare(const struct task *task);
int user_mode_enter_stub(const struct task *task);
int user_mode_run_smoke_test(struct task *task);
int user_mode_handle_exception(unsigned long esr,
                               unsigned long elr,
                               unsigned long far,
                               unsigned long spsr);
int user_mode_handle_exit_syscall(unsigned long code);
const struct address_space *user_mode_active_address_space(void);
const char *user_mode_status_name(int status);
const char *user_mode_entry_state(const struct task *task);
void arm64_enter_el0(unsigned long pc, unsigned long sp, unsigned long spsr);
void arm64_enter_el0_smoke(unsigned long pc,
                           unsigned long sp,
                           unsigned long spsr);
void arm64_el0_smoke_return(void);

#endif
