# SaturnOS

SaturnOS is a small ARM64 operating system built from the ground up in C and
AArch64 assembly.

The project is currently focused on early kernel foundations: boot, UART,
exceptions, timer interrupts, kernel threads, preemptive scheduling, a QEMU
framebuffer console, physical memory management, a small kernel heap, and
ARM64 identity-mapped virtual memory.

## Current Status

Version: 0.5.4

Codename: Memory

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
- Optional demo kernel threads for scheduler testing

### Graphics

- Graphical QEMU launch script
- QEMU `ramfb` framebuffer setup through `fw_cfg`
- 640x480 XRGB8888 framebuffer
- Pixel drawing and test pattern
- Built-in bitmap text rendering
- Kernel console output mirrored to both UART and framebuffer
- Basic framebuffer scrolling

### Input

- UART-backed keyboard input polling
- Keyboard input kernel thread
- Typed characters echoed through UART and framebuffer console

### Shell

- Line-based kernel shell
- Built-in `help`, `version`, `tasks`, `mem`, `heap`, `heaptest`, `vm`,
  `ticks`, `clear`, and `panic` commands

### Memory Management

- Physical memory manager for QEMU `virt` RAM
- 4 KiB page bitmap allocator
- Kernel image reservation using linker symbols
- QEMU ramfb memory reservation
- `pmm_alloc_page` and `pmm_free_page` page APIs
- Shell `mem` command for memory diagnostics
- Page-backed kernel heap allocator
- `kmalloc`, `kzalloc`, and `kfree` APIs
- Shell `heap` and `heaptest` commands for heap diagnostics
- ARM64 page-table constants and descriptor helpers
- Identity-map plan for QEMU `virt` RAM and MMIO
- Static L1/L2 translation tables with 2 MiB block mappings
- MAIR_EL1, TCR_EL1, TTBR0_EL1, and SCTLR_EL1 MMU enable path
- ARM64 MMU enabled with an identity map
- Shell `vm` command for virtual-memory diagnostics

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
kernel/input/      Keyboard input thread
kernel/memory/     Physical memory manager
kernel/panic/      Kernel panic diagnostics
kernel/sched/      Kernel scheduler
kernel/shell/      Interactive kernel shell
kernel/src/        Kernel entry point
scripts/           QEMU run scripts
docs/              Architecture, roadmap, and graphics notes
```

## Current Boot Flow

1. Initialize UART.
2. Initialize QEMU framebuffer if available.
3. Initialize physical memory management.
4. Initialize the kernel heap.
5. Initialize virtual-memory planning.
6. Initialize exceptions, timer, IRQ, and scheduler.
7. Print boot diagnostics to UART and framebuffer.
8. Start optional demo kernel threads when enabled.
9. Start keyboard shell input thread.
10. Enable periodic timer interrupts.
11. Run preemptive kernel thread scheduling.

## Next Milestones

- Improve framebuffer console text wrapping and cursor behavior
- Add line editing for keyboard input
- Add page-table permission refinements for kernel text, data, and MMIO
- Expand scheduler robustness and task management

## Vision

SaturnOS aims to grow into a complete operating system with its own kernel,
device drivers, scheduler, memory manager, filesystem, networking stack,
graphical environment, and developer tools.
