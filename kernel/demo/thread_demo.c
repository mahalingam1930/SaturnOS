#include "thread_demo.h"
#include "config.h"
#include "kprintf.h"
#include "scheduler.h"
#include "timer.h"

static void demo_thread_a(void)
{
    int counter = 0;

    while (counter < 4)
    {
        counter++;
        if (CONFIG_THREAD_DEMO_LOG)
        {
            kprintf("Thread A iteration %d\n", counter);
        }
        timer_sleep_ms(250);
    }

    if (CONFIG_THREAD_DEMO_LOG)
    {
        kprintf("Thread A returning\n");
    }
}

static void demo_thread_b(void)
{
    int counter = 0;

    while (counter < 8)
    {
        counter++;
        if (CONFIG_THREAD_DEMO_LOG)
        {
            kprintf("Thread B iteration %d\n", counter);
        }
        timer_sleep_ms(250);
    }

    if (CONFIG_THREAD_DEMO_LOG)
    {
        kprintf("Thread B returning\n");
    }
}

void thread_demo_init(void)
{
    scheduler_create_kernel_thread("thread-a", demo_thread_a);
    scheduler_create_kernel_thread("thread-b", demo_thread_b);
}
