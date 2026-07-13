# SaturnOS

SaturnOS is a small ARM64 operating system built from the ground up in C and
AArch64 assembly.

The project is currently focused on early kernel foundations: boot, UART,
exceptions, timer interrupts, kernel threads, preemptive scheduling, a QEMU
framebuffer console, physical memory management, a small kernel heap, and
ARM64 identity-mapped virtual memory.

## Current Status

Version: 0.6.20

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
- Task memory metadata foundation
- Shared kernel address-space object
- Per-task stack and guard range diagnostics
- Per-task address-space diagnostics
- User/process address-space scaffold
- User address-space page-table storage scaffold
- Controlled user code, data, and stack mapping plan
- User code/data/stack descriptor permission scaffold
- User/kernel permission split metadata
- Controlled user descriptor installation scaffold
- EL0 entry state preparation metadata
- User address-space validation diagnostics
- Guarded EL0 transition stub
- User address-space switching preparation metadata
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
  `vmwalk`, `ticks`, `clear`, `panic`, and `fault` commands

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
- VM diagnostics for executable and execute-never mappings
- Device/MMIO regions marked execute-never
- Page-fault diagnostics for instruction and data aborts
- Decoded fault status code, access type, and fault level
- Translation-table validation before enabling the MMU
- VM diagnostics for validated blocks and validation errors
- Shell hex/decimal argument parsing for VM page walks
- Named VM region lookup for `vmwalk`
- Kernel section linker symbols for `.text`, `.rodata`, `.data`, and `.bss`
- VM diagnostics for kernel section ranges
- VM permission diagnostics for kernel sections and MMIO
- Permission hardening status reporting for pending finer-grained mappings
- L3 page mappings for the kernel's first 2 MiB RAM block
- Execute-never enforcement for kernel `.rodata` and `.bss`
- Read-only page permissions for kernel `.rodata`
- VM diagnostics for execute and write permissions
- Execute-never default for normal RAM blocks
- Heap and scheduler stack execute-never diagnostics
- Read-only executable kernel `.text` pages
- Unmapped guard pages around scheduler stacks
- VM validation for mapped pages and unmapped stack guards
- VM security summary for code, data, heap, stacks, guards, and MMIO
- VM named range diagnostics for kernel, heap, stacks, framebuffer, and MMIO
- Shared kernel address-space object for scheduler tasks
- User/process address-space object scaffold
- Static user address-space L1 table pool for future processes
- Controlled user virtual-memory ranges for future processes
- User descriptor policy for code, data, and stack permissions
- Explicit EL1-only kernel and EL0-capable user permission policy
- User-owned L1/L2/L3 table hierarchy for controlled descriptors
- Task EL0 PC, SP, SPSR, and readiness diagnostics
- Address-space validation status and error diagnostics
- User-mode transition status diagnostics without `eret`
- Active and target root-table switching diagnostics
- Shell `vm` and `vmwalk` commands for virtual-memory diagnostics

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
- Add EL0 exception-return implementation
- Add guarded TTBR0 switch stub
- Expand scheduler robustness and task management

## Vision

SaturnOS aims to grow into a complete operating system with its own kernel,
device drivers, scheduler, memory manager, filesystem, networking stack,
graphical environment, and developer tools.
