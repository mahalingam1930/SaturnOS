# SaturnFS

SaturnFS is the first native on-disk SaturnOS filesystem. It uses the common
block API and therefore runs on both virtio-blk and the RAM-disk fallback.

## Layout

```text
sector 0       superblock
sector 1       eight fixed directory entries
sector 2+      eight fixed 4 KiB file extents
```

Each directory entry stores an absolute path, starting sector, byte size,
payload checksum, and allocation flag. Files are limited to 4 KiB. SaturnFS
formats only a blank device automatically; unknown non-empty formats are left
untouched and reported as unavailable.

## Shell Commands

- `pfs` shows mount state and persistent files.
- `pfs format` explicitly reformats the filesystem.
- `pwrite <path> <text>` creates or replaces a persistent file.
- `pcat <path>` verifies its checksum and prints its contents.

Persistence was verified by writing a file to the QEMU virtio disk, restarting
the VM, remounting SaturnFS, and reading the same contents back.

## VFS Mount

SaturnFS is mounted at `/disk`. VFS translates `/disk/path` to SaturnFS
`/path`, so standard shell file commands and executable loading work without a
filesystem-specific call path. The built-in executable is mirrored to
`/disk/bin/user-demo.sx` during boot.
