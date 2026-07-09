#include "irq.h"
#include "kprintf.h"
#include "timer.h"

#define GICD_BASE 0x08000000UL
#define GICC_BASE 0x08010000UL

#define GICD_CTLR        0x000
#define GICD_ISENABLER0  0x100
#define GICD_IPRIORITYR  0x400

#define GICC_CTLR 0x000
#define GICC_PMR  0x004
#define GICC_IAR  0x00C
#define GICC_EOIR 0x010

#define TIMER_IRQ 30

static inline void mmio_write(unsigned long address, unsigned int value)
{
    *(volatile unsigned int *)address = value;
}

static inline void mmio_write8(unsigned long address, unsigned char value)
{
    *(volatile unsigned char *)address = value;
}

static inline unsigned int mmio_read(unsigned long address)
{
    return *(volatile unsigned int *)address;
}

void irq_init(void)
{
    mmio_write(GICD_BASE + GICD_CTLR, 0);

    mmio_write8(GICD_BASE + GICD_IPRIORITYR + TIMER_IRQ, 0x80);
    mmio_write(GICD_BASE + GICD_ISENABLER0, (1U << TIMER_IRQ));

    mmio_write(GICD_BASE + GICD_CTLR, 1);

    mmio_write(GICC_BASE + GICC_PMR, 0xFF);
    mmio_write(GICC_BASE + GICC_CTLR, 1);
}

void irq_enable(void)
{
    __asm__ volatile("msr daifclr, #2");
}

void irq_disable(void)
{
    __asm__ volatile("msr daifset, #2");
}

void irq_handler(void)
{
    unsigned int iar = mmio_read(GICC_BASE + GICC_IAR);
    unsigned int irq = iar & 0x3FF;

    if (irq == TIMER_IRQ)
    {
        timer_handle_irq();
    }
    else
    {
        kprintf("Unhandled IRQ: %d\n", irq);
    }

    mmio_write(GICC_BASE + GICC_EOIR, iar);
}
