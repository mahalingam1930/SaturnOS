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
- Task memory metadata with stack, guard, and root-table diagnostics

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

- shared kernel identity-map root table
- usable stack range
- lower and upper stack guard ranges

This does not create separate process address spaces yet. It makes the current
shared kernel map explicit in task diagnostics and prepares the scheduler for
future per-task address-space objects.

## Next MMU Work

1. Add task address-space objects.
2. Prepare task/process memory separation beyond the shared kernel map.
3. Decide when to enable instruction and data caches.
