# SaturnOS Scheduler

This document tracks the current scheduler and shell-level task management
commands.

## Current Status

SaturnOS has a preemptive kernel-thread scheduler for ARM64 QEMU `virt`.
The scheduler owns a fixed task table, per-task kernel stacks, stack guard
ranges, task memory metadata, and early user-task lifecycle state.

Current scheduler capabilities:

- idle task
- kernel thread creation
- ARM64 context switching
- timer-driven preemption
- cooperative `scheduler_yield()`
- task exit to zombie state
- per-task stack and guard metadata
- per-task address-space metadata
- blocked user-task creation path
- controlled user-task admission checks
- EL0 BRK smoke test and recovery counters

## Task States

```text
unused     task slot is unused
ready      runnable task waiting for CPU time
running    currently selected task
blocked    task is not eligible to run
eligible   user task passed checks but is not scheduler-runnable yet
zombie     task has completed and should not run again
```

The scheduler currently only selects `ready` and `running` tasks. The
`eligible` state is used for controlled user-mode smoke testing without making
the user-shaped task part of normal runnable selection.

## Shell Commands

### `task`

Prints a compact scheduler summary:

```text
saturn> task
Task summary: ticks=64 tasks=3 current=1
  pid=0 name=idle state=ready run=yes kind=kernel
  pid=1 name=keyboard state=running run=yes kind=kernel
  pid=2 name=user-demo state=zombie run=no kind=user
```

### `task <pid>`

Prints focused details for one task:

```text
saturn> task 2
Task 2:
  name=user-demo state=zombie policy=inactive runnable=no current=no
  aspace=user-demo kind=user root=0x400d2000 target=0x400c8000
  switch=ready check=ok errors=0
  stack=0x4009d000-0x4009e000
  guards=0x4009c000-0x4009d000 0x4009e000-0x4009f000
  el0=yes pc=0x100000 sp=0x40000000 spsr=0x0
  user_entry=ready status=ready
```

Alias:

```text
tl -> task
```

### `tasks`

Prints the full verbose scheduler dump. This is still useful for low-level VM,
address-space, guard-page, and EL0 readiness inspection.

Aliases:

```text
ps  -> tasks
top -> tasks
```

### `yield`

Voluntarily yields the current scheduler task:

```text
saturn> yield
Yielding scheduler task
Scheduler task resumed
```

Alias:

```text
y -> yield
```

### `sleep <ms>`

Sleeps the current shell task using the timer path:

```text
saturn> sleep 100
Sleeping 100 ms
Sleep done
```

The shell clamps long sleeps to keep the interactive console responsive:

```text
Max sleep: 10000 ms
```

Alias:

```text
nap -> sleep
```

### `ticks`

Shows scheduler and timer interrupt counters:

```text
saturn> ticks
scheduler ticks=39 timer irqs=39
```

Alias:

```text
uptime -> ticks
```

### `user`

Shows focused user/EL0 smoke counters:

```text
saturn> user
User exception stats:
  task 2: user-demo state=zombie
    smoke=completed result=passed
    admit=1 enter=1 trap=1 recover=1
    reject=0 complete=1 fail=0
    entry=ready status=ready
```

Alias:

```text
ustats -> user
```

## Next Scheduler Work

- Add real task sleep states instead of timer busy-wait sleep.
- Add blocking and unblocking APIs for kernel tasks.
- Add task cleanup or slot reuse for zombie tasks.
- Add scheduler accounting, such as per-task runtime and switches.
- Add syscall-backed yield and exit once user programs are real.
