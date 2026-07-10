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

## Planned Identity Map

```text
0x08000000 - 0x0a000000  device MMIO
0x40000000 - 0x48000000  normal RAM
```

## Built Tables

- Root L1 table: one 4 KiB table
- L2 tables: one for low MMIO, one for RAM
- MMIO mapping: 16 x 2 MiB device blocks
- RAM mapping: 64 x 2 MiB normal-memory blocks
- Total mapped: 160 MiB

## Permission Model

- Device/MMIO blocks: execute-never
- RAM blocks: executable for now because kernel text, rodata, data, heap, and
  stacks still share coarse 2 MiB mappings
- Next step: split kernel regions into smaller mappings so text can be
  executable/read-only while data, heap, and stacks are execute-never

## MMU Configuration

- Translation granule: 4 KiB
- VA size: 32-bit TTBR0 address space
- Root lookup level: L1
- Normal memory MAIR index: 0
- Device memory MAIR index: 1
- RAM mapping: executable for now because kernel regions share 2 MiB blocks
- Caches: left disabled for the first MMU milestone

## Validation

Before enabling the MMU, SaturnOS validates:

- L1 and L2 table alignment
- L1 table descriptors for every mapped region
- L2 block descriptors for every mapped block
- Expected physical address for each 2 MiB block
- Validated block count against mapped block count

## Page Walk Diagnostics

The `vmwalk` shell command walks representative virtual addresses through the
current L1/L2 tables. It also accepts an explicit address and labels known
regions such as `uart`, `kernel`, `framebuffer`, and `gap`:

- `0x09000000`: UART MMIO mapping
- `0x40080000`: kernel image mapping
- `0x20000000`: unmapped gap

```text
vmwalk 0x40080000
vmwalk 0x09000000
vmwalk 0x20000000
```

## Next MMU Work

1. Split kernel text, rodata, data, heap, and stack permissions.
2. Expand named VM regions as new drivers and memory ranges appear.
3. Decide when to enable instruction and data caches.
