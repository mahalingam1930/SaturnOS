# SaturnOS Syscalls

SaturnOS has an early syscall dispatcher foundation for future EL0 programs.
The current implementation is intentionally kernel-side first: it defines
syscall numbers, dispatches them through one kernel function, records basic
statistics, and exposes shell diagnostics.

## Current Syscall Numbers

```text
1  write   args: fd, buffer, length
2  exit    args: code
3  yield   args: none
```

## Current Behavior

- `write` records the call and currently returns the requested length.
- `exit` records the call and currently returns success.
- `yield` records the call and performs a scheduler yield.
- unknown syscall numbers are rejected with `-1`.

These are stubs. They give the kernel a stable dispatch point before real user
program ABI plumbing is connected.

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

The lower-EL exception path already decodes SVC exceptions, but the exception
entry code still needs to pass a saved register frame into C so the kernel can
read EL0 syscall arguments from `x0`-`x7` and return a result to `x0`.

The next syscall milestone is to wire EL0 `svc` exceptions into
`syscall_dispatch()`, then replace the stubs with real user-visible behavior.
