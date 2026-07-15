# SaturnOS Syscalls

SaturnOS has an early syscall path for future EL0 programs. The current
implementation defines syscall numbers, dispatches them through one kernel
function, records basic statistics, exposes shell diagnostics, and routes
lower-EL AArch64 `svc` exceptions into the dispatcher.

## Current Syscall Numbers

```text
1  write   args: fd, buffer, length
2  exit    args: code
3  yield   args: none
4  open    args: path, length
5  read    args: fd, buffer, length
6  close   args: fd
7  create  args: path, length
8  seek    args: fd, absolute offset
```

## Current Behavior

- `write` accepts console descriptors `1` and `2` or an open task-owned file
  descriptor, validates a bounded user buffer, advances file offsets, and
  returns the number of bytes written.
- `exit` records the requested exit code and completes the active EL0 program
  back to its scheduler context in EL1.
- `yield` records the call and performs a scheduler yield.
- `open` copies a bounded path from user memory, resolves it through VFS, and
  returns a descriptor owned by the current task.
- `read` copies at most 128 bytes into a validated writable user range and
  advances the descriptor offset.
- `close` releases a valid descriptor; all descriptors are also cleared when
  the task slot is reaped or reused.
- `create` copies a bounded path from user memory, creates or truncates the VFS
  file, and returns a task-owned descriptor positioned at offset zero.
- `seek` sets a valid task-owned descriptor to an absolute offset from zero
  through the current end of file and returns the new offset.
- unknown syscall numbers are rejected with `-1`.

Program completion is independent of the diagnostic BRK fallback. Synchronous
EL0 faults are recovered as failed tasks rather than successful exits.

## EL0 ABI

For lower-EL AArch64 `svc #0` traps:

```text
x8      syscall number
x0-x3   syscall arguments
x0      return value
```

The exception vector saves general-purpose registers, passes the saved frame to
C, dispatches the syscall, writes the result into saved `x0`, and returns to
the `ELR_EL1` address supplied by the CPU for the SVC trap.

At initial program entry, `x0` contains the optional argument-text length and
`x1` points to its NUL-terminated copy in the task's writable user data page.
The shell accepts `run <path> [argument text]`, caps the text at 128 bytes, and
never exposes the shell command buffer or another task's memory to EL0.

## Boot Smoke Test

The `user-demo` smoke image now executes:

```text
mov x8, #1      syscall: write
mov x0, #1      arg0: fd
mov x1, #0x200000 arg1: user data buffer
mov x2, #23        arg2: length
svc #0
mov x8, #2         syscall: exit
mov x0, #7         arg0: exit code
svc #0
brk #0             fallback if exit returns to EL0
```

The SVC prints `hello from EL0 syscall`, proving the EL0 syscall path can pass
a user buffer to the kernel and return to the next user instruction. The exit
syscall then completes the program back to EL1 with exit code `7`. The final
BRK remains a fault fallback if exit ever incorrectly returns to EL0.

## Shell Diagnostics

Use the shell command:

```text
syscall
```

to print the syscall table and dispatch counters.

Use:

```text
syscall 3
```

to test dispatching the `yield` syscall stub.

`sc` is a short alias for `syscall`.

## Filesystem Integration Test

`run /bin/user-file.sx` opens `/disk/syscall.txt`, reads its persistent contents
into the writable user data page, closes the descriptor, prints the bytes
through console `write`, and exits with code `0`. Invalid pointer and descriptor
tests return `-2` and `-1` without corrupting task state.

`run /bin/user-file-write.sx` creates or truncates `/disk/user-write.txt`,
writes `written from EL0`, closes the descriptor, and exits with code `0`. The
file remains readable after a full QEMU reboot, exercising the descriptor,
VFS, SaturnFS rewrite, and checksum paths together.

`run /bin/user-file-seek.sx` opens `/disk/syscall.txt`, seeks to byte `6`,
reads from the new offset, and prints only `from file syscall` before closing
the descriptor and exiting with code `0`.

## User Image Loading

User code and optional data are loaded from a Saturn executable in the RAM
filesystem, then copied into controlled user pages. The loader validates the
magic, format version, header size, code and data bounds, aligned entry offset,
exact file size, and payload checksum. The built-in image is stored as
`/bin/user-demo.sx`.

`run /bin/user-args.sx Saturn argument works` exercises the entry ABI by
writing the exact argument text through the console syscall and exiting with
code `0`. Launching the fixture without text verifies the zero-length path.
