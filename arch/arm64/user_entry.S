.section .text
.global arm64_enter_el0
.global arm64_enter_el0_task
.global arm64_el0_task_return

/*
 * void arm64_enter_el0(unsigned long pc, unsigned long sp,
 *                      unsigned long spsr)
 *
 * x0 = ELR_EL1 target
 * x1 = SP_EL0 target
 * x2 = SPSR_EL1 target
 */
arm64_enter_el0:
    msr ELR_EL1, x0
    msr SP_EL0, x1
    msr SPSR_EL1, x2
    isb
    eret

arm64_enter_el0_task:
    msr ELR_EL1, x0
    msr SP_EL0, x1
    msr SPSR_EL1, x2
    isb
    eret

arm64_el0_task_return:
    ret
