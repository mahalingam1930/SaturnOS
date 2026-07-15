# SaturnOS

SaturnOS is a small ARM64 operating system built from the ground up in C and
AArch64 assembly.

The project is currently focused on early kernel foundations: boot, UART,
exceptions, timer interrupts, kernel threads, preemptive scheduling, a QEMU
framebuffer console, physical memory management, a small kernel heap, and
ARM64 identity-mapped virtual memory.

## Current Status

Version: 0.6.76

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
- Focused scheduler task summary and per-task status diagnostics
- Shell command for voluntary scheduler yields
- Shell command for bounded timer-backed sleeps
- Scheduler-backed sleeping task state with timer wakeups
- Safe scheduler block and unblock APIs for kernel tasks
- Zombie task cleanup and tail slot reuse
- Per-task scheduler accounting for switches and run ticks
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
- EL0 exception-return assembly path
- Guarded TTBR0 switch stub
- First blocked user-task creation path
- User-mode smoke task image scaffold
- Controlled user task unblock path
- Deliberate EL0 syscall smoke test and recovery
- User smoke task completion cleanup
- Hardened EL0 recovery tuple validation
- Per-user-task lifecycle counters
- Focused shell diagnostics for user/EL0 exception statistics
- Syscall dispatcher foundation with write, exit, and yield syscall IDs
- EL0 SVC exception path wired into the syscall dispatcher
- User smoke image that writes a user-buffer message through SVC before exit
  recovery
- Bounded syscall `write` validation and console output for user buffers
- User `exit` syscall completion path with tracked exit code
- Direct scheduler-runnable user task context for the EL0 smoke path
- Bounded in-memory user image loader for code, data, and entry-point setup
- File-backed user image loading through the kernel VFS
- Per-task user address-space ownership with page-table slot reclamation
- Repeatable RAMFS user-program launch through the `run` shell command
- Validated Saturn executable headers with payload checksum verification
- Writable RAMFS files with truncation and parent-validated directories
- Sector-based block-device API with a RAM-disk reference backend
- Non-destructive block read/write self-test and operation diagnostics
- Legacy and modern virtio-MMIO block driver with RAM-disk fallback
- Persistent 4 MiB QEMU disk-image launch configuration
- SaturnFS fixed-layout persistent files with payload checksums
- Persistent file inspection and writes through `pfs`, `pcat`, and `pwrite`
- SaturnFS mounted at `/disk` through the common VFS read/write interface
- Persistent `.sx` executable loading through ordinary VFS paths
- Generic EL0 run sessions completed by `exit`, independent of BRK recovery
- Contained EL0 fault recovery with failed-task retirement and reuse
- Per-task user file descriptors with validated `open`, `read`, and `close`
- EL0 file-reading program backed by persistent `/disk` VFS data
- Validated user `create` and descriptor-backed `write` syscalls
- EL0 persistent-file write program with reboot persistence verification
- Validated descriptor seek syscall and EL0 persistent-file seek program
- Bounded user-program argument copying with EL0 `x0`/`x1` entry ABI
- Bounded multi-argument EL0 `argc`/`argv`/length-vector entry ABI
- Non-blocking user task wait/status syscall with safe zombie reaping
- Validated user task spawn syscall with VFS executable loading
- Bounded argument propagation from EL0 parent to spawned child
- Parent-owned child wait authorization and orphan-safe reparenting
- Shell syscall diagnostics and dispatcher test command
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
- Framebuffer console line wrapping and cursor-bound handling
- Expanded framebuffer glyph coverage for shell and diagnostic punctuation
- Polished framebuffer L-cursor with bounds-safe hide/show behavior
- Framebuffer console header and cleaner diagnostic layout
- Framebuffer shell/status line for input, VM, and EL0 smoke state
- Shell framebuffer runtime status command

### Input

- UART-backed keyboard input polling
- QEMU virtio keyboard input for graphical framebuffer sessions
- Keyboard input kernel thread
- Typed characters echoed through UART and framebuffer console

### Shell

- Line-based kernel shell
- Cursor-aware shell line editing with insert, backspace, delete, home/end,
  and left/right movement
- Fixed-size shell command history with up/down navigation
- Tab autocomplete for unique built-in command prefixes
- Tab completion match listing for ambiguous built-in command prefixes
- Short shell aliases for common diagnostics
- Per-command usage details through `help <command>`
- Built-in `help`, `version`, `task`, `tasks`, `mem`, `heap`, `heaptest`,
  `vm`, `vmwalk`, `ticks`, `yield`, `sleep`, `block`, `unblock`, `fb`,
  `user`, `syscall`, `ls`, `cat`, `run`, `mkdir`, `write`, `reap`, `clear`,
  `disk`, `pfs`, `pcat`, `pwrite`, `panic`, and `fault` commands

### Filesystem

- VFS create, lookup, bounded read/write, truncate, and file-data interfaces
- Static RAM filesystem with bounded paths, directories, files, and file sizes
- Built-in `/bin/user-demo.sx` executable and `/share/user-demo.txt` data file
- Shell file listing and text output through `ls` and `cat`
- Shell directory creation and file replacement through `mkdir` and `write`

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
- Guarded `eret` path for future EL0 entry
- Guarded TTBR0 switch primitive and diagnostics
- Blocked user task with validated user address-space diagnostics
- User code page SVC/BRK smoke image, entry, size, and checksum diagnostics
- User task eligibility state that remains non-runnable
- Shared EL1-only kernel mappings inside user address spaces
- Expected lower-EL SVC write and exit recovery back to EL1
- User smoke completion result diagnostics
- Named EL0 recovery rejection diagnostics
- User admission, entry, trap, recovery, completion, and failure counters
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
and the QEMU framebuffer window. Click the QEMU framebuffer window to send
keyboard input through the virtio keyboard path; terminal input still works
through UART.

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
kernel/syscall/    Syscall dispatcher foundation
kernel/src/        Kernel entry point
scripts/           QEMU run scripts
docs/              Architecture, roadmap, and graphics notes
```

Additional docs:

- [Scheduler](docs/SCHEDULER.md)
- [Syscalls](docs/SYSCALLS.md)

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

- Add blocking wait semantics for child tasks

## Vision

SaturnOS aims to grow into a complete operating system with its own kernel,
device drivers, scheduler, memory manager, filesystem, networking stack,
graphical environment, and developer tools.
