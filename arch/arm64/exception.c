#include "exception.h"
#include "cpu.h"
#include "decoder.h"
#include "panic.h"
#include "syscall.h"
#include "user.h"

#define ESR_EC_SHIFT 26
#define ESR_EC_MASK 0x3FUL
#define ESR_EC_SVC64 0x15UL
#define SPSR_MODE_MASK 0xFUL

void exception_init(void)
{
    __asm__ volatile (
        "adr x0, exception_vector\n"
        "msr VBAR_EL1, x0\n"
        "isb\n"
        :
        :
        : "x0"
    );
}

void exception_test(void)
{
    __asm__ volatile("brk #0");
}

void exception_test_page_fault(void)
{
    volatile unsigned long *unmapped = (volatile unsigned long *)0x20000000UL;
    volatile unsigned long value = *unmapped;
    (void)value;
}

static unsigned long exception_class_value(unsigned long esr)
{
    return (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;
}

static int exception_from_el0(unsigned long spsr)
{
    return (spsr & SPSR_MODE_MASK) == ARM64_SPSR_MODE_EL0T;
}

static void exception_read_system_state(unsigned long *esr,
                                        unsigned long *elr,
                                        unsigned long *far,
                                        unsigned long *spsr)
{
    __asm__ volatile("mrs %0, ESR_EL1" : "=r"(*esr));
    __asm__ volatile("mrs %0, ELR_EL1" : "=r"(*elr));
    __asm__ volatile("mrs %0, FAR_EL1" : "=r"(*far));
    __asm__ volatile("mrs %0, SPSR_EL1" : "=r"(*spsr));
}

void exception_sync_handler(struct exception_frame *frame)
{
    unsigned long esr;
    unsigned long elr;
    unsigned long far;
    unsigned long spsr;
    struct exception_info info;

    exception_read_system_state(&esr, &elr, &far, &spsr);
    decode_exception_info(esr, &info);

    if (frame &&
        exception_from_el0(spsr) &&
        exception_class_value(esr) == ESR_EC_SVC64)
    {
        long result = syscall_dispatch(frame->x8,
                                       frame->x0,
                                       frame->x1,
                                       frame->x2,
                                       frame->x3);

        frame->x0 = (unsigned long)result;
        return;
    }

    if (user_mode_handle_exception(esr, elr, far, spsr))
    {
        return;
    }

    kernel_panic(
        &info,
        esr,
        elr,
        far,
        spsr
    );
}

void exception_handler(void)
{
    unsigned long esr;
    unsigned long elr;
    unsigned long far;
    unsigned long spsr;

    exception_read_system_state(&esr, &elr, &far, &spsr);

    struct exception_info info;
    decode_exception_info(esr, &info);

    if (user_mode_handle_exception(esr, elr, far, spsr))
    {
        return;
    }

    kernel_panic(
        &info,
        esr,
        elr,
        far,
        spsr
    );
}
