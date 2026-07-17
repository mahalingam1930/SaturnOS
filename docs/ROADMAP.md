# SaturnOS Roadmap

## Current Version

SaturnOS is currently at `0.6.94` and targets ARM64 QEMU `virt`.

## Completed

### Kernel Foundation

- ARM64 boot path
- linker script and freestanding build
- UART console
- `kprintf`
- exception vectors and diagnostics
- panic path

### Core Kernel

- ARM generic timer
- GICv2 interrupt handling
- physical memory manager
- kernel heap
- ARM64 MMU identity map
- kernel memory permission diagnostics
- page-fault diagnostics

### Scheduler And Tasks

- kernel task table
- idle task
- kernel thread creation
- ARM64 context switching
- timer-driven preemption
- cooperative yield
- bounded shell sleep command
- real scheduler sleeping task state
- safe kernel-task block and unblock APIs
- zombie task cleanup and tail slot reuse
- per-task scheduler accounting
- task summary and per-task status shell commands
- user-shaped task scaffold
- EL0 syscall smoke test and recovery
- user lifecycle counters
- syscall dispatcher foundation for write, exit, and yield
- EL0 SVC exception path wired into syscall dispatch
- syscall write validation and console output for user buffers
- syscall exit completion path with tracked exit code
- direct scheduler-runnable user task context for the EL0 smoke path
- bounded in-memory user image loading with validated code, data, and entry

### Graphics And Input

- QEMU graphical launch path
- QEMU `ramfb` framebuffer setup
- 640x480 XRGB8888 framebuffer
- bitmap text rendering
- UART and framebuffer console mirroring
- framebuffer status line
- UART keyboard input
- QEMU virtio keyboard input

### Shell Diagnostics

- line editing
- history
- tab completion
- aliases
- per-command help
- memory, heap, VM, scheduler, framebuffer, user, and syscall diagnostics

### Filesystem And Program Loading

- VFS create, lookup, bounded read, and direct file-data interfaces
- bounded static RAM filesystem
- built-in program and data files
- `ls` and `cat` shell diagnostics
- file-backed loading of user code and data through VFS paths
- per-task user address spaces with reclaimable page-table slots
- repeatable user-program launch and task-slot reuse
- validated Saturn executable headers and payload checksums
- parent-validated RAMFS directories and bounded writable files
- `mkdir` and `write` shell commands
- bounded sector block API and RAM-disk reference backend
- non-destructive block-device self-test and I/O counters
- legacy and modern virtio-MMIO block transport
- persistent QEMU raw disk image with RAM-disk fallback
- SaturnFS superblock, fixed directory, checksummed files, and reboot remount
- persistent filesystem shell diagnostics and file operations
- SaturnFS mounted at `/disk` through the common VFS namespace
- persistent executable loading through standard VFS paths
- generic EL0 exit-completion sessions independent of BRK
- contained EL0 fault recovery and failed-task retirement
- deliberate faulting executable lifecycle test
- per-task user file descriptor tables
- validated user `open`, `read`, and `close` syscalls
- EL0 persistent-file read integration program and rejection tests
- validated user `create` and descriptor-backed `write` syscalls
- EL0 persistent-file write integration and reboot persistence test
- validated descriptor seek syscall and EL0 seek integration program
- bounded shell-to-EL0 user-program argument ABI and integration program
- bounded multi-argument vector ABI with count, pointers, and lengths
- validated non-blocking user wait/status syscall and zombie reaping
- validated EL0 spawn syscall with VFS loading and child admission
- bounded spawned-child argument propagation into the existing argv ABI
- parent-child ownership, wait authorization, and orphan reparenting
- explicit EL0 yield guard preventing recovery-session corruption
- task-indexed EL0 recovery sessions and address-space-aware resume
- blocking child wait with ELR/SPSR preservation and zombie reaping
- wait-for-any-owned-child selection and no-child rejection
- validated wait `NOHANG` polling and unknown-option rejection
- parent-owned child termination and waitable failure completion
- user sleep with ELR/SPSR/TTBR restoration and EL0 timer ownership guard
- monotonic millisecond syscall and shell-visible timing diagnostics
- current and parent process identity syscalls
- validated user system-information snapshots with version, memory, and task data
- nonnegative user pseudo-random values mixed with scheduler and task state
- authorized process-status snapshots for the caller and its owned children
- bounded user enumeration of RAM filesystem paths, sizes, and node kinds
- validated user directory creation with bounded path copying
- validated user removal of RAM filesystem files and empty directories
- atomic path rename with collision and self-descendant protections
- validated path metadata snapshots across RAMFS and mounted SaturnFS files
- explicit bounded user file truncation across RAMFS and SaturnFS

## Next

### User Space

- add user descriptor-duplication syscall

### Storage

- later add FAT32 or ext2

## Future

- mouse input
- USB
- networking
- window manager
- GUI toolkit
- desktop applications
