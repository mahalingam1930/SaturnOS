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
- scheduler-backed sleeping task state
- guarded kernel-task block and unblock APIs
- zombie task cleanup and tail slot reuse
- per-task scheduler accounting
- task exit to zombie state
- per-task stack and guard metadata
- per-task address-space metadata
- blocked user-task creation path
- controlled user-task admission checks
- EL0 syscall smoke test, exit code, and recovery counters
- direct scheduler-runnable user-demo task context

## Task States

```text
unused     task slot is unused
ready      runnable task waiting for CPU time
running    currently selected task
blocked    task is not eligible to run
sleeping   task is blocked until a scheduler tick reaches its wake time
eligible   user task passed checks but is not scheduler-runnable yet
zombie     task has completed and should not run again
```

The scheduler currently selects `ready` and `running` tasks. User tasks still
pass through controlled admission checks, then the user-demo task becomes
`ready` and enters EL0 from its own scheduled task context.

## Shell Commands

### `task`

Prints a compact scheduler summary:

```text
saturn> task
Task summary: ticks=64 tasks=3 current=1
  pid=0 name=idle state=ready run=yes kind=kernel sw=1 ticks=0
  pid=1 name=keyboard state=running run=yes kind=kernel sw=8 ticks=6
  pid=2 name=user-demo state=zombie run=no kind=user sw=0 ticks=0
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
  account switches=0 run_ticks=0 yields=0 preempt=0 wakeups=0
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

Blocks the current shell task until the scheduler tick reaches its wake time:

```text
saturn> sleep 100
Sleeping 100 ms (blocked)
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

### `block <pid>`

Blocks a safe kernel task by pid. The command refuses to block the idle task,
the current shell task, user tasks, sleeping tasks, and zombie tasks.

```text
saturn> block 0
Failed to block task 0: idle task is protected
```

Alias:

```text
blk -> block
```

### `unblock <pid>`

Returns a manually blocked kernel task to `ready`. The command refuses idle,
user tasks, and tasks that are not currently blocked.

```text
saturn> unblock 1
Failed to unblock task 1: state=running
```

Alias:

```text
unblk -> unblock
```

### `reap [pid]`

Reclaims zombie task slots. With no argument, `reap` cleans every zombie task
that is not the current task. With a pid, it only reaps that task.

```text
saturn> reap 2
Reaped task 2 (user-demo)
```

Alias:

```text
rz -> reap
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
    reject=0 exit=1 code=7 complete=1 fail=0
    entry=ready status=ready
```

Alias:

```text
ustats -> user
```

## Accounting

Each task tracks:

```text
switches    times the scheduler selected the task to run
run_ticks   timer ticks charged while the task was current
yields      yield-driven switches away from the task
preempt     timer preemptions charged to the task
wakeups     timer wakeups from scheduler sleep
```

The `task` command shows compact switch and run-tick counters. `task <pid>` and
`tasks` show the full accounting tuple.

## Next Scheduler Work

- Generalize the controlled EL0 lifecycle beyond the initial smoke protocol.
