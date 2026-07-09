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
    kprintf("Welcome to %s\n", SATURNOS_NAME);
    kprintf("Version %s\n", SATURNOS_VERSION);
    kprintf("Codename %s\n", SATURNOS_CODENAME);
    kprintf("Drive: %c\n", 'A');
    kprintf("CPU: %d\n", 0);
    kprintf("Memory: %d MB\n", 512);
    kprintf("Temperature: %d C\n", -5);
    kprintf("UART Base: 0x%x\n", 0x09000000);
    kprintf("Magic Number: 0x%x\n", 0xDEADBEEF);
    kprintf("================================\n");

    kprintf("Generic Timer Frequency: %d Hz\n", (int)timer_get_frequency());
    kprintf("Timer Tick Start: 0x%x\n", (unsigned int)timer_get_ticks());
    kprintf("Waiting 100 ms...\n");
    timer_sleep_ms(100);
    kprintf("Timer Tick End: 0x%x\n", (unsigned int)timer_get_ticks());
    kprintf("Timer test complete.\n");

    thread_demo_init();
    scheduler_dump_tasks();

    timer_start_periodic(100);
    irq_enable();
    scheduler_start_threads();

    while (1)
    {
    }
}
