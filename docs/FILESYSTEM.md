# SaturnOS Filesystem

SaturnOS provides an early VFS interface backed by a bounded RAM filesystem.
It supports directories, file creation, bounded offset-based reads and writes,
truncation, direct kernel access to file data, and node-list diagnostics.

## Limits

```text
files       8
directories 8
path        47 characters plus terminator
file size   4096 bytes
```

Paths are absolute and parent directories must exist. Duplicate paths,
oversized files, invalid buffers, and reads outside a file are rejected. Reads
that cross end-of-file are shortened. Writes may extend a file within its 4 KiB
limit, and truncation supports replacement without stale trailing bytes.

## Built-In Files

```text
/bin/user-demo.sx          Saturn executable image
/share/user-demo.txt       syscall write data
```

The scheduler resolves both files through VFS and passes their contents to the
user image loader. Each launched task owns its address-space object and returns
its page-table slot when reaped. This exercises repeatable file-backed program
loading before persistent storage is available.

## Shell Commands

`ls` lists RAM filesystem nodes and sizes. `cat <path>` prints a file and
reports missing paths without changing filesystem state. `mkdir <path>` adds a
directory, and `write <path> <text>` creates or replaces a text file. `run`
launches the built-in program in a new user task.

SaturnFS is mounted at `/disk`. The same `ls`, `cat`, `write`, and `run`
interfaces route `/disk/...` paths to persistent storage. For example,
`run /disk/bin/user-demo.sx` loads and validates an executable after a reboot.

The `pfs`, `pcat`, and `pwrite` commands remain available as low-level
SaturnFS administration and diagnostics.
