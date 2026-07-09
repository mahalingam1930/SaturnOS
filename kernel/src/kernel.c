#include "uart.h"
#include "kprintf.h"
#include "exception.h"
#include "timer.h"
#include "irq.h"
#include "scheduler.h"
#include "thread_demo.h"
#include "version.h"

void kernel_main(void)
{
    uart_init();

    exception_init();
    timer_init();
    irq_init();
    scheduler_init();

    kprintf("================================\n");
    kprintf("%s %s (%s)\n",
            SATURNOS_NAME,
            SATURNOS_VERSION,
            SATURNOS_CODENAME);
    kprintf("Target : ARM64 QEMU virt\n");
    kprintf("UART   : initialized at 0x%x\n", 0x09000000);
    kprintf("IRQ    : GICv2 initialized\n");
    kprintf("Timer  : %d Hz\n", (int)timer_get_frequency());
    kprintf("Sched  : preemptive kernel threads\n");
    kprintf("================================\n");

    kprintf("Timer sanity: start=0x%x\n", (unsigned int)timer_get_ticks());
    timer_sleep_ms(100);
    kprintf("Timer sanity: end=0x%x\n", (unsigned int)timer_get_ticks());

    thread_demo_init();
    scheduler_dump_tasks();

    timer_start_periodic(100);
    irq_enable();
    scheduler_start_threads();

    while (1)
    {
    }
}
