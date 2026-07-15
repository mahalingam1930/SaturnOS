# Saturn Executable Format

SaturnOS uses a compact executable container for early EL0 programs. Files use
the `.sx` suffix and contain one fixed header followed by code and data bytes.

## Header

```text
magic              0x53415458 (SATX)
version            1
header_size        fixed header byte size
code_size          executable payload bytes
data_size          writable payload bytes
entry_offset       aligned offset inside code
payload_checksum   checksum over code followed by data
```

The loader rejects incorrect magic or version values, malformed sizes,
oversized sections, unaligned or out-of-range entry offsets, trailing or
missing bytes, and checksum mismatches. Code and data are limited to one mapped
4 KiB page each in the current address-space implementation.

The shell command `run [path]` loads an image through VFS. With no path it runs
`/bin/user-demo.sx`.
