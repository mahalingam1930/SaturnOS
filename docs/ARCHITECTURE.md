# SaturnOS Architecture

## Name

SaturnOS

## Version

0.6.47

## Target Architecture

ARM64 AArch64 on QEMU `virt`

## Goal

Build a small operating system from scratch with a real kernel foundation,
memory manager, scheduler, shell, graphics path, and eventually user programs,
filesystems, networking, and a desktop environment.

## Kernel Type

Monolithic Kernel

## Components

- Boot entry and linker script
- ARM64 CPU, MMU, exception, IRQ, timer, and context-switch code
- PL011 UART driver
- QEMU `ramfb` framebuffer driver
- QEMU virtio keyboard input path
- Console and `kprintf`
- Panic and exception diagnostics
- Physical memory manager
- Page-backed kernel heap
- Virtual-memory diagnostics and page-walk tooling
- Kernel scheduler and task table
- User address-space and EL0 smoke-test scaffold
- Interactive kernel shell

## Boot Flow

1. Initialize UART.
2. Initialize QEMU framebuffer if available.
3. Initialize physical memory management.
4. Initialize the kernel heap.
5. Initialize exceptions, timer, IRQ, scheduler, keyboard, and VM state.
6. Create the keyboard input thread.
7. Create and validate the controlled user-demo task.
8. Run the EL0 BRK smoke test.
9. Start periodic timer interrupts.
10. Enter preemptive kernel-thread scheduling.

## Scheduler Model

The scheduler currently uses a fixed task table with explicit task states:

```text
unused, ready, running, blocked, eligible, zombie
```

It supports an idle task, kernel thread creation, timer-driven preemption,
cooperative yield, task exit, and shell-visible diagnostics. User-shaped tasks
can be admitted and smoke-tested, but normal user process scheduling and
syscalls are still future work.

See `docs/SCHEDULER.md` for scheduler shell commands and task-management notes.

## Memory Model

SaturnOS currently uses an ARM64 identity map for QEMU RAM and MMIO. The kernel
has page-level permission diagnostics for text, rodata, data, bss, heap,
scheduler stacks, stack guards, framebuffer, and MMIO regions.

User/process address spaces exist as a scaffold with controlled code, data, and
stack descriptors. The EL0 smoke path proves controlled entry and exception
recovery, but not yet general-purpose user programs.

## Graphics And Input

The graphical path uses QEMU `ramfb` with a 640x480 XRGB8888 framebuffer.
Kernel output is mirrored to UART and framebuffer. The framebuffer console has
bitmap text rendering, scrolling, line wrapping, cursor handling, and status
bands. Input currently comes from UART and QEMU virtio keyboard.
