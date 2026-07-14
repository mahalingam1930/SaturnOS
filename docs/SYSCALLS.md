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
```

## Current Behavior

- `write` accepts file descriptors `1` and `2`, validates a bounded user
  buffer, writes bytes to the kernel console, and returns the number of bytes
  written.
- `exit` records the requested exit code and completes the active controlled
  EL0 smoke run back to EL1.
- `yield` records the call and performs a scheduler yield.
- unknown syscall numbers are rejected with `-1`.

`exit` is still intentionally scoped to the guarded smoke path; the next step is
normal scheduler-driven user-task execution.

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
syscall then completes the smoke run back to EL1 with exit code `7`. The final
BRK remains as a fallback if exit ever incorrectly returns to EL0.

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

## Next Work

The next syscall milestone is to promote user tasks from scheduler-driven
runner execution to direct scheduler-runnable EL0 contexts.
