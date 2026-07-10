# SaturnOS Virtual Memory Plan

This document tracks the ARM64 virtual-memory foundation. SaturnOS now builds
the initial identity translation tables, but does not enable the MMU yet.

## Current Scope

- 4 KiB translation granule constants
- L1, L2, and L3 index helpers
- Block and page descriptor helper functions
- Identity-map tables for QEMU `virt` MMIO and RAM
- Static L1/L2 tables
- 2 MiB L2 block descriptors
- Shell `vm` command for diagnostics

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

## Next MMU Enable Checklist

1. Fill MAIR_EL1 with normal and device memory attributes.
2. Configure TCR_EL1 for the selected address size and granule.
3. Point TTBR0_EL1 at the root L1 table.
4. Flush TLBs and instruction caches.
5. Enable SCTLR_EL1.M only after the identity map is verified.
6. Keep UART, GIC, timer, ramfb, kernel text/data, stacks, and heap mapped.
