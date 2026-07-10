# SaturnOS

SaturnOS is a small ARM64 operating system built from the ground up in C and
AArch64 assembly.

The project is currently focused on early kernel foundations: boot, UART,
exceptions, timer interrupts, kernel threads, preemptive scheduling, and a
QEMU framebuffer console.

## Current Status

Version: 0.4.0

Codename: Scheduler

Target: ARM64 QEMU `virt`

## Completed Milestones

### Boot and Console

- ARM64 kernel boot on QEMU `virt`
- Custom linker script and boot entry
- PL011 UART driver
- Kernel console abstraction
- `kprintf` support for strings, chars, decimals, and hex values

### Exceptions and Panic

- ARM64 exception vector table
- Exception handler framework
- ESR_EL1, ELR_EL1, FAR_EL1, and SPSR_EL1 diagnostics
- Kernel panic screen over UART
- ESR exception-class decoder

### Timer and IRQ

- ARM generic timer initialization
- GICv2 IRQ setup
- Periodic timer interrupts
- Timer sanity checks during boot

### Scheduler

- Scheduler foundation
- ARM64 context-switch foundation
- Cooperative kernel threads
- Thread trampoline and task exit handling
- Timer-driven preemptive scheduling
- Real idle task
- Kernel thread creation API
- Thread demo module

### Graphics

- Graphical QEMU launch script
- QEMU `ramfb` framebuffer setup through `fw_cfg`
- 640x480 XRGB8888 framebuffer
- Pixel drawing and test pattern
- Built-in bitmap text rendering
- Kernel console output mirrored to both UART and framebuffer
- Basic framebuffer scrolling

## Build

```sh
make
```

Clean build artifacts:

```sh
make clean
```

## Run

UART-only terminal mode:

```sh
./scripts/run.sh
```

Graphical framebuffer mode:

```sh
./scripts/run-gui.sh
```

In graphical mode, SaturnOS writes kernel output to both the terminal UART path
and the QEMU framebuffer window.

## Project Layout

```text
arch/arm64/        ARM64 CPU, exceptions, IRQ, timer, context switching
boot/              Boot entry and linker script
drivers/uart/      PL011 UART driver
drivers/video/     QEMU ramfb framebuffer driver
kernel/console/    Console and kprintf
kernel/demo/       Kernel thread demo
kernel/exception/  ESR decoder
kernel/panic/      Kernel panic diagnostics
kernel/sched/      Kernel scheduler
kernel/src/        Kernel entry point
scripts/           QEMU run scripts
docs/              Architecture, roadmap, and graphics notes
```

## Current Boot Flow

1. Initialize UART.
2. Initialize QEMU framebuffer if available.
3. Initialize exceptions, timer, IRQ, and scheduler.
4. Print boot diagnostics to UART and framebuffer.
5. Start demo kernel threads.
6. Enable periodic timer interrupts.
7. Run preemptive kernel thread scheduling.

## Next Milestones

- Improve framebuffer console text wrapping and cursor behavior
- Add keyboard input
- Build an interactive kernel shell
- Begin memory management foundations
- Expand scheduler robustness and task management

## Vision

SaturnOS aims to grow into a complete operating system with its own kernel,
device drivers, scheduler, memory manager, filesystem, networking stack,
graphical environment, and developer tools.
