#include "exception.h"
#include "decoder.h"
#include "panic.h"
#include "user.h"

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

void exception_handler(void)
{
    unsigned long esr;
    unsigned long elr;
    unsigned long far;
    unsigned long spsr;

    __asm__ volatile("mrs %0, ESR_EL1" : "=r"(esr));
    __asm__ volatile("mrs %0, ELR_EL1" : "=r"(elr));
    __asm__ volatile("mrs %0, FAR_EL1" : "=r"(far));
    __asm__ volatile("mrs %0, SPSR_EL1" : "=r"(spsr));

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
