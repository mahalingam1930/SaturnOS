# SaturnOS Roadmap

## Current Version

SaturnOS is currently at `0.6.57` and targets ARM64 QEMU `virt`.

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

## Next

### User Space

- load user programs from an in-memory source

### Storage

- add VFS interfaces
- add RAM filesystem
- add file-backed user program loading
- later add disk and FAT32 or ext2

## Future

- mouse input
- USB
- networking
- window manager
- GUI toolkit
- desktop applications
