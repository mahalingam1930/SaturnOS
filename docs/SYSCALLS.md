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
9  wait    args: pid, status buffer, options
10 spawn   args: path, path length, argument text, argument length
11 terminate args: child PID, termination code
12 sleep   args: milliseconds
13 monotonic_ms args: none
14 getpid  args: none
15 getppid args: none
16 system_info args: writable information buffer
17 random args: none
```

## Current Behavior

- `write` accepts console descriptors `1` and `2` or an open task-owned file
  descriptor, validates a bounded user buffer, advances file offsets, and
  returns the number of bytes written.
- `exit` records the requested exit code and completes the active EL0 program
  back to its scheduler context in EL1.
- `yield` performs a scheduler yield for kernel and EL0 callers. Recovery state
  is indexed per task, and the scheduler restores the resumed task's TTBR0 root
  before returning through its suspended exception frame.
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
- `wait` blocks cooperatively while an owned child is active, then copies the
  completed PID, exit code, and success flag to validated user memory and reaps
  the zombie. The yield path preserves TTBR0, `ELR_EL1`, and `SPSR_EL1` across
  child execution. Invalid or unrelated PIDs return `-1`; bad pointers `-2`.
  Passing PID `0` selects any owned child, preferring an already completed
  zombie; it returns `-1` immediately when the caller owns no children.
  Option bit `1` (`NOHANG`) returns `0` immediately for active children;
  unknown option bits return `-1`.
- `spawn` copies a bounded path and optional argument text from user memory,
  loads and validates the executable through VFS, packs the child's argument
  vectors, admits the child task, and returns its PID.
- `terminate` lets a parent cancel an owned non-zombie child. The child becomes
  a failed zombie with the requested code and remains available to `wait`.
  Self, unrelated, invalid, and already-completed targets return `-1`.
- `sleep` suspends the caller for 1 through 60,000 milliseconds and restores
  its user TTBR0 and exception-return registers on wake. Timer IRQs wake and
  account EL0 tasks without changing logical ownership absent a context switch.
- `monotonic_ms` returns scheduler ticks converted through the configured timer
  interval. It requires no user pointer and never moves backward during boot.
- `getpid` returns the current scheduler task ID. `getppid` returns the recorded
  spawning parent ID, or `0` for shell/kernel-owned user tasks.
- `system_info` writes a fixed snapshot containing the kernel version, page
  size, total and free physical pages, scheduler ticks, current task count, and
  task capacity. The entire output range must be writable; bad pointers return
  `-2` without partially updating user memory.
- `random` returns a nonnegative 31-bit pseudo-random value. Its xorshift state
  is mixed with scheduler ticks and the caller task ID; it is suitable for
  ordinary user-program variability, not cryptographic use.
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

At initial program entry, `x0` contains `argc`, `x1` points to an array of
NUL-terminated argument pointers, and `x2` points to the matching length array.
The shell accepts `run <path> [arguments]`, caps input at 128 bytes and eight
space-delimited arguments, and stores every vector and string inside the
task's writable user data page.

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

Launching `/bin/user-demo.sx` as task 3 followed by `/bin/user-wait.sx`
exercises completed-status delivery and safe zombie reaping; the waiter prints
`wait ok` only when syscall 9 returns the completed state.

`run /bin/user-spawn.sx` invokes syscall 10 for `/bin/user-args.sx` with two
arguments. The parent exits with code `0`, then the admitted child prints
`child args`, proving the copied argument vector is independent of its parent.
The scheduler records the spawning PID and reparents any remaining children
when their parent exits, preventing stale ownership after task-slot reuse.
The spawn integration also terminates its child before entry with code `99`,
then confirms the status through wait; terminated child code never executes.

## User Image Loading

User code and optional data are loaded from a Saturn executable in the RAM
filesystem, then copied into controlled user pages. The loader validates the
magic, format version, header size, code and data bounds, aligned entry offset,
exact file size, and payload checksum. The built-in image is stored as
`/bin/user-demo.sx`.

`run /bin/user-args.sx alpha beta` exercises the vector ABI by independently
loading and writing the first two argument entries. Launching without arguments
verifies `argc = 0`; a ninth argument is rejected before task admission.
