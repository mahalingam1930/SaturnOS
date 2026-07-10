# SaturnOS Virtual Memory Plan

This milestone defines the ARM64 virtual-memory foundation without enabling
the MMU yet.

## Current Scope

- 4 KiB translation granule constants
- L1, L2, and L3 index helpers
- Block and page descriptor helper functions
- Identity-map plan for QEMU `virt` MMIO and RAM
- Shell `vm` command for diagnostics

## Planned Identity Map

```text
0x08000000 - 0x0a000000  device MMIO
0x40000000 - 0x48000000  normal RAM
```

## Next MMU Enable Checklist

1. Allocate aligned translation tables.
2. Fill MAIR_EL1 with normal and device memory attributes.
3. Configure TCR_EL1 for the selected address size and granule.
4. Point TTBR0_EL1 at the root table.
5. Flush TLBs and instruction caches.
6. Enable SCTLR_EL1.M only after the identity map is verified.
7. Keep UART, GIC, timer, ramfb, kernel text/data, stacks, and heap mapped.
