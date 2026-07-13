# SaturnOS Virtual Memory Plan

This document tracks the ARM64 virtual-memory foundation. SaturnOS now builds
the initial identity translation tables and enables the MMU with an identity
map.

## Current Scope

- 4 KiB translation granule constants
- L1, L2, and L3 index helpers
- Block and page descriptor helper functions
- Identity-map tables for QEMU `virt` MMIO and RAM
- Static L1/L2 tables
- 2 MiB L2 block descriptors
- MAIR_EL1, TCR_EL1, TTBR0_EL1, and SCTLR_EL1 setup
- MMU enabled with TTBR0 identity mappings
- Execute-permission diagnostics
- Device/MMIO regions marked execute-never
- Page-fault diagnostics for instruction and data aborts
- Translation-table validation before enabling the MMU
- Shell `vm` and `vmwalk` commands for diagnostics
- Hex/decimal shell argument parsing for `vmwalk`
- Named VM region lookup for page-walk output
- Linker-provided kernel section ranges for `.text`, `.rodata`, `.data`,
  and `.bss`
- Permission diagnostics for kernel sections and MMIO
- L3 page table for the 2 MiB block containing the kernel image
- Execute-never page enforcement for kernel `.rodata` and `.bss`
- Read-only page enforcement for kernel `.rodata`
- Execute and write-permission diagnostics for page walks
- Execute-never default for normal RAM blocks
- Heap and scheduler stack execute-never diagnostics
- Read-only executable kernel `.text` pages
- Unmapped guard pages around scheduler stacks
- Validation for intentionally unmapped guard pages
- VM security summary for high-level protection posture
- VM named range diagnostics for kernel, heap, stacks, framebuffer, and MMIO
- Shared kernel address-space object
- User/process address-space object scaffold
- Static user address-space L1 table pool
- Controlled user code, data, and stack mapping plan
- User code/data/stack descriptor permission scaffold
- User/kernel permission split metadata
- Controlled user descriptor installation scaffold
- EL0 entry state preparation metadata
- User address-space validation diagnostics
- Task memory metadata with stack, guard, address-space, and root-table
  diagnostics

## Planned Identity Map

```text
0x08000000 - 0x0a000000  device MMIO
0x40000000 - 0x48000000  normal RAM
```

## Built Tables

- Root L1 table: one 4 KiB table
- L2 tables: one for low MMIO, one for RAM
- MMIO mapping: 16 x 2 MiB device blocks
- RAM mapping: 63 x 2 MiB normal-memory blocks plus 512 x 4 KiB pages for
  the kernel block
- Total mapped: 160 MiB

## Permission Model

- Device/MMIO blocks: execute-never
- Kernel `.text` pages: executable and read-only
- Kernel `.rodata` pages: execute-never and read-only
- Kernel `.data` pages: execute-never when present
- Kernel `.bss` pages: execute-never
- Heap pages: execute-never and writable
- Scheduler stack pages: execute-never and writable
- Scheduler guard pages: unmapped
- Remaining RAM blocks: execute-never by default

The `vm` shell command reports desired and actual permissions:

```text
VM security:
  code     exec/ro
  rodata   xn/ro
  data     xn/rw
  heap     xn/rw
  stacks   xn/rw
  guards   unmapped
  mmio     xn/rw

VM ranges:
  kernel.text  0x40080000-0x...
  kernel.ro    0x...-0x...
  kernel.bss   0x...-0x...
  heap         0x...-0x...
  stack0       0x...-0x...
  stack.area   0x...-0x...
  guard.first  0x...-0x...
  guard.last   0x...-0x...
  framebuffer  0x47000000-0x4712c000
  gic          0x08000000-0x08020000
  uart         0x09000000-0x09001000

VM protect: granularity=4 KiB kernel pages
  text exec=exec/exec enforced write=ro/ro enforced
  rodata exec=xn/xn enforced write=ro/ro enforced
  data exec=xn/empty empty write=rw/empty empty
  bss exec=xn/xn enforced write=rw/rw enforced
  mmio exec=xn/xn enforced write=rw/rw enforced
  heap exec=xn/xn enforced write=rw/rw enforced
  stacks exec=xn/xn enforced write=rw/rw enforced
  guards pages=9 actual=unmapped status=enforced
```

The broad RAM region may still report mixed permissions because the first
2 MiB RAM block is split into executable and execute-never L3 pages while the
rest of RAM remains mapped with execute-never L2 blocks.

## Kernel Section Symbols

The linker script exports section boundaries for the kernel image:

- `.text`: executable kernel code, mapped read-only
- `.rodata`: read-only constants and strings, mapped read-only
- `.data`: initialized writable data
- `.bss`: zeroed writable data, tables, and stacks

The VM diagnostics use these symbols to label kernel addresses and assign page
permissions more precisely.

## MMU Configuration

- Translation granule: 4 KiB
- VA size: 32-bit TTBR0 address space
- Root lookup level: L1
- Normal memory MAIR index: 0
- Device memory MAIR index: 1
- Kernel mapping: 4 KiB L3 pages inside the first 2 MiB RAM block
- Remaining RAM mapping: execute-never 2 MiB blocks
- Caches: left disabled for the first MMU milestone

## Validation

Before enabling the MMU, SaturnOS validates:

- L1 and L2 table alignment
- L3 table alignment for the kernel page table
- L1 table descriptors for every mapped region
- L2 block descriptors for every mapped block
- L3 page descriptors for the protected kernel block
- Intentionally unmapped scheduler stack guard pages
- Expected physical address for each 2 MiB block
- Expected physical address for each 4 KiB kernel page
- Validated block, page, and guard counts against mapped counts

## Page Walk Diagnostics

The `vmwalk` shell command walks representative virtual addresses through the
current L1/L2/L3 tables. It also accepts an explicit address and labels known
regions such as `uart`, `kernel-text`, `kernel-rodata`, `kernel-data`,
`kernel-bss`, `framebuffer`, and `gap`:

- `0x09000000`: UART MMIO mapping
- `0x40080000`: kernel text mapping
- `0x20000000`: unmapped gap

```text
vmwalk 0x40080000
vmwalk 0x09000000
vmwalk 0x20000000
```

## Task Memory Foundation

Each scheduler task now carries memory metadata:

- shared kernel address-space object
- shared kernel identity-map root table through that object
- usable stack range
- lower and upper stack guard ranges

This does not create separate process address spaces yet. It makes the current
shared kernel map explicit in task diagnostics and prepares the scheduler for
future per-task user address spaces.

## User Address-Space Scaffold

SaturnOS now has a first-class address-space type for future user/process
memory:

- kernel address spaces represent the current shared kernel identity map
- user address spaces carry a planned user range
- user address spaces still share the protected kernel map
- user address spaces can own L1 page-table storage
- user address spaces reserve controlled code, data, and stack ranges
- user address spaces carry intended descriptor permissions for each user
  region
- kernel address spaces explicitly deny EL0 access
- future user address spaces allow EL0 access only for controlled user regions
- user address spaces install controlled descriptors into private L3 tables
- user page-table mappings are marked as not ready until the real per-process
  tables exist

Current kernel threads continue to use the shared kernel address space. The
scaffold is intentionally structural: it gives future process creation, syscall,
and EL0 work a place to attach isolated user mappings.

User address spaces keep two roots during this phase:

- `root_table`: the safe active kernel root used until switching is ready
- `user_root_table`: private L1 table storage for future user mappings

This prevents an empty user table from being activated too early while still
making page-table ownership explicit in the address-space model.

The planned user layout is:

```text
0x00100000 - 0x00200000  user code
0x00200000 - 0x00300000  user data
0x3fff0000 - 0x40000000  user stack
```

These ranges are policy metadata only in this milestone. SaturnOS reports the
plan through address-space diagnostics, but the mappings are still inactive
until real user descriptors and EL0 permissions are added.

The planned descriptor policy is:

```text
user code   readable, executable, read-only, privileged-execute-never
user data   readable, writable, execute-never
user stack  readable, writable, execute-never
```

The descriptor metadata is attached to address-space objects. SaturnOS can now
install representative controlled descriptors into user-owned tables, but it
does not switch current kernel threads into those tables.

## User/Kernel Permission Split

SaturnOS now records the intended privilege split in address-space metadata:

```text
kernel memory  EL1 only, EL0 blocked
user code      EL0 readable/executable, EL1 execute-never
user data      EL0 readable/writable, execute-never
user stack     EL0 readable/writable, execute-never
```

Current kernel threads still run entirely in the kernel address space, so their
task diagnostics report `k_el0=no`, `u_el0=no`, and inactive user mappings.
Future user/process address spaces will set `u_el0=yes` while keeping
`k_el0=no`.

## User Descriptor Installation

Future user address spaces now own:

```text
1 x L1 table
1 x L2 table
3 x L3 tables
3 x one-page backing regions
```

The installed scaffold maps one controlled page for each planned user region:

```text
code   first page of user code range
data   first page of user data range
stack  first page of user stack range
```

This proves that SaturnOS can build the table hierarchy and attach descriptors
with the planned permissions. The active scheduler still uses the safe kernel
root table until EL0 entry and address-space switching are ready.

## EL0 Entry Preparation

Scheduler tasks now carry future user-entry state:

```text
EL0 PC    first user code address
EL0 SP    top of user stack, 16-byte aligned
SPSR_EL1  EL0t return mode
ready     whether the task can safely enter EL0 later
```

Kernel threads still run in EL1, so their diagnostics report `el0=no`.
Future user tasks only become `el0=yes` after they have a user address space
with installed user mappings and executable user code metadata.

## EL0 Transition Stub

SaturnOS now has a guarded user-mode transition API:

```text
user_mode_can_enter(task)
user_mode_prepare(task)
user_mode_enter_stub(task)
```

The stub checks task, address-space, validation, PC, SP, and SPSR readiness,
then returns a status string. Scheduler diagnostics use the prepare/check path
only, so boot diagnostics do not execute `eret`, switch TTBR0, or leave EL1.
Kernel threads report `user_entry=blocked` with status `kernel-task`, which is
the expected safe state.

## EL0 Exception-Return Path

SaturnOS now has the low-level assembly doorway for future EL0 entry:

```text
arm64_enter_el0(pc, sp, spsr)
```

The routine writes `ELR_EL1`, `SP_EL0`, and `SPSR_EL1`, then executes `eret`.
Normal kernel boot does not reach this path because scheduler diagnostics use
the prepare/check path only, and the first user-shaped task remains blocked.

## User Address-Space Switching Preparation

Address spaces now track switching metadata:

```text
active_root  currently safe root table
target_root  future root table for the address space
switch       active, ready, or blocked
ready        whether switching can be attempted later
```

Kernel address spaces report `switch=active` because they already run on the
kernel root. Future validated user address spaces can report `switch=ready`
when their user root table is prepared and differs from the active kernel root.
SaturnOS still does not write TTBR0 for user spaces in this milestone.

## Guarded TTBR0 Switch Stub

The MMU layer now exposes a low-level TTBR0 switch primitive:

```text
arm64_mmu_switch_ttbr0(root)
```

Address spaces also expose a guarded switch stub that validates the target root
and reports `ttbr0_stub=active`, `ready`, or `blocked`. Current kernel tasks
remain on the active kernel root and only exercise the diagnostic stub.

## Blocked User Task Path

The scheduler can now create a first user-shaped task object:

```text
task name   user-demo
state       blocked
aspace      user
el0         ready metadata only
```

This task receives a validated user address space with controlled code, data,
and stack descriptors. It is intentionally marked `blocked`, and the scheduler
runnable selection only chooses `ready` or `running` tasks. This lets SaturnOS
prove the user-task creation path without entering EL0 or running user code.

## User-Mode Smoke Task Scaffold

SaturnOS now places a tiny smoke image into the user code page for `user-demo`.
The image is currently one AArch64 `BRK` instruction. That gives the future
EL0 entry path a deterministic first instruction while still making accidental
execution immediately visible through the exception path.

The user-entry readiness check now requires:

```text
validated user address space
installed user mappings
installed user smoke image
valid EL0 PC/SP/SPSR metadata
```

Expected diagnostics include:

```text
user_image=ready entry=0x100000 size=4 checksum=...
user_entry=ready status=ready
state=blocked
```

The task is still blocked, so this milestone prepares the user payload but does
not schedule it or execute `eret`.

Expected diagnostics:

```text
task N: user-demo state=blocked
aspace=user-demo kind=user
user_tables=yes user_desc=yes user_map=yes
user_image=ready entry=0x100000 size=4
aspace_check=ok errors=0
switch=ready
ttbr0_stub=ready
el0=yes
user_entry=ready status=ready
```

## Controlled User Task Unblock Path

The scheduler now has an explicit user-task unblock gate:

```text
scheduler_unblock_user_task(pid)
```

The gate only succeeds when the task is currently blocked, its EL0 entry state
is ready, its user smoke image is installed, and the guarded TTBR0 switch stub
reports `ready`. On success, the task moves to `state=eligible`.

`eligible` is deliberately separate from `ready`:

```text
ready      scheduler may run this task
eligible   user task passed checks, but scheduler must not run it yet
blocked    task is not eligible
```

Expected diagnostics:

```text
User task N (user-demo) is eligible; runnable=no
task N: user-demo state=eligible
policy=user-eligible runnable=no
user_image=ready entry=0x100000 size=4
user_entry=ready status=ready
```

This proves the user task can pass the controlled admission checks while still
preventing accidental EL0 entry.

## Deliberate EL0 BRK Smoke Test

SaturnOS now performs its first controlled EL0 entry. The user-demo task enters
EL0 at its smoke image, executes the expected `BRK`, traps back to EL1, and
returns to kernel code without a panic.

The user address space now shares the kernel and MMIO mappings as EL1-only
entries, while the user code, data, and stack pages remain the only EL0-capable
regions. This lets the lower-EL exception vector, kernel stack, and UART path
remain reachable after switching `TTBR0_EL1` to the user root table.

Expected boot flow:

```text
Starting EL0 BRK smoke test for task N (user-demo)
EL0 smoke: entering user task at 0x100000
EL0 smoke: caught expected BRK at ELR=0x100000
EL0 smoke: returned to EL1
User task N (user-demo) smoke=passed
SaturnOS shell ready
```

Only the expected lower-EL BRK is handled this way. Unexpected exceptions still
fall through to the normal kernel panic diagnostics.

## User Address-Space Validation

Address spaces now report validation status and error counts. The validator
checks:

```text
table storage      present for future user spaces
descriptor counts  expected and installed descriptors match
permission split   kernel EL0 access blocked
region policy      code exec/ro, data+stack rw/xn
EL0 readiness      executable user mapping is available
```

Kernel address spaces validate as `ok` with zero errors. Future user address
spaces must pass this validation before SaturnOS should attempt address-space
switching or EL0 entry.

## Next MMU Work

1. Add user task cleanup after EL0 smoke completion.
2. Add EL0 exception return-to-kernel recovery hardening.
3. Decide when to enable instruction and data caches.
