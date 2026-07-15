# SaturnOS Block Storage

SaturnOS exposes a sector-oriented block API with bounded reads and writes.
Backends include a virtio-MMIO block device and a 64 KiB RAM-disk fallback.
The launch scripts attach a persistent 4 MiB raw QEMU disk image. Both legacy
and modern virtio-MMIO transports are supported.

## API

```text
block_read(sector, buffer, count)
block_write(sector, buffer, count)
```

Requests reject null buffers, zero-sector operations, overflow, and ranges
outside the device. Diagnostics track requests, transferred sectors, and
rejections.

The `disk` shell command displays capacity and counters. `disk test` preserves
the final sector, writes and verifies a deterministic pattern, then restores
the original bytes.

If no virtio block device is attached, SaturnOS falls back to the RAM backend,
which persists for the current boot only. The next storage milestone is an
on-disk filesystem over the shared sector interface.
