# SaturnOS Architecture

## Name

SaturnOS

## Version

0.6.76

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
- VFS and bounded RAM filesystem
- sector block-device layer and RAM-disk backend
- virtio-MMIO block backend for persistent QEMU disk images
- SaturnFS compact native on-disk filesystem
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
- Syscall dispatcher foundation
- Interactive kernel shell

## Boot Flow

1. Initialize UART.
2. Initialize QEMU framebuffer if available.
3. Initialize physical memory management.
4. Initialize the kernel heap.
5. Initialize exceptions, timer, IRQ, scheduler, keyboard, and VM state.
6. Create the keyboard input thread.
7. Create and validate the controlled user-demo task.
8. Admit the user-demo task as a scheduler-runnable task.
9. Start periodic timer interrupts.
10. Enter preemptive kernel-thread scheduling.
11. The scheduled user-demo task enters EL0 from its task context.

## Scheduler Model

The scheduler currently uses a fixed task table with explicit task states:

```text
unused, ready, running, blocked, eligible, zombie
```

It supports an idle task, kernel thread creation, timer-driven preemption,
cooperative yield, task exit, and shell-visible diagnostics. User-shaped tasks
can be admitted and run. The syscall dispatcher provides console write, exit,
yield, and per-task VFS open/read/close operations. Lower-EL SVC exceptions are routed into the
dispatcher using `x8` as the syscall number and `x0`-`x3` as arguments, but
the `write` syscall can validate and print bounded user buffers, and `exit`
can complete a generic EL0 run session with a tracked exit code. The
user-demo task is now admitted as a direct scheduler-runnable context that
enters EL0 from its own task entry; loading arbitrary user programs is still
future work.

See `docs/SCHEDULER.md` for scheduler shell commands and task-management notes.

## Memory Model

SaturnOS currently uses an ARM64 identity map for QEMU RAM and MMIO. The kernel
has page-level permission diagnostics for text, rodata, data, bss, heap,
scheduler stacks, stack guards, framebuffer, and MMIO regions.

User/process address spaces provide controlled code, data, and stack
descriptors. EL0 programs complete through `exit`; synchronous user faults are
contained, recorded as program failures, and returned to the scheduler without
panicking the kernel.

## Graphics And Input

The graphical path uses QEMU `ramfb` with a 640x480 XRGB8888 framebuffer.
Kernel output is mirrored to UART and framebuffer. The framebuffer console has
bitmap text rendering, scrolling, line wrapping, cursor handling, and status
bands. Input currently comes from UART and QEMU virtio keyboard.
