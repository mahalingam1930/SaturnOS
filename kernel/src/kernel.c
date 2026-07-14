#include "uart.h"
#include "kprintf.h"
#include "exception.h"
#include "timer.h"
#include "irq.h"
#include "scheduler.h"
#include "thread_demo.h"
#include "version.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "keyboard_input.h"
#include "config.h"
#include "pmm.h"
#include "heap.h"
#include "address_space.h"
#include "vm.h"

void kernel_main(void)
{
    int user_demo_pid;

    uart_init();

    if (framebuffer_init())
    {
        framebuffer_console_init();
    }

    pmm_init();
    heap_init();
    exception_init();
    vm_init();
    address_space_init(vm_root_table());
    timer_init();
    irq_init();
    scheduler_init();
    keyboard_init();

    if (framebuffer_is_ready())
    {
        framebuffer_console_set_status(
            keyboard_graphical_ready()
                ? "READY | UART+KBD | MMU ON | VM OK"
                : "READY | UART | MMU ON | VM OK");
    }

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
    kprintf("Memory : %d KB free\n",
            (int)((pmm_free_pages() * PMM_PAGE_SIZE) / 1024));
    kprintf("Heap   : initialized\n");
    kprintf("Input  : %s\n", keyboard_source());
    kprintf("VM     : %s, tables %s, check %s\n",
            vm_state(),
            vm_table_state(),
            vm_validation_state());
    if (framebuffer_is_ready())
    {
        kprintf("Video  : ramfb 640x480\n");
    }
    else
    {
        kprintf("Video  : UART only (fb status=%d)\n", framebuffer_status());
    }
    kprintf("================================\n");

    kprintf("Timer sanity: start=0x%x\n", (unsigned int)timer_get_ticks());
    timer_sleep_ms(100);
    kprintf("Timer sanity: end=0x%x\n", (unsigned int)timer_get_ticks());

    if (CONFIG_THREAD_DEMO_ENABLE)
    {
        thread_demo_init();
    }
    keyboard_input_init();
    user_demo_pid = scheduler_create_blocked_user_task("user-demo");
    scheduler_unblock_user_task(user_demo_pid);
    scheduler_create_user_smoke_runner(user_demo_pid);
    scheduler_dump_tasks();
    if (framebuffer_is_ready())
    {
        framebuffer_console_set_status(
            keyboard_graphical_ready()
                ? "READY | UART+KBD | MMU ON | EL0 SCHEDULED"
                : "READY | UART | MMU ON | EL0 SCHEDULED");
    }

    timer_start_periodic(100);
    irq_enable();
    scheduler_start_threads();

    while (1)
    {
    }
}
