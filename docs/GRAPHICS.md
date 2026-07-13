# SaturnOS Graphics

## Current Status

SaturnOS currently renders kernel output through the PL011 UART serial console.
The default runner uses QEMU's `-nographic` mode, so it intentionally does not
open a display window.

For a graphical QEMU window with the UART console attached to the window, run:

```sh
./scripts/run-gui.sh
```

SaturnOS also has an early QEMU `ramfb` framebuffer path. When available, the
kernel configures a 640x480 XRGB8888 framebuffer and mirrors the kernel console
to both UART and the framebuffer.

The framebuffer console supports basic bitmap text rendering, shell and
diagnostic punctuation glyphs, scrolling, line wrapping, cursor-bound handling,
and a bounds-safe L-style cursor. It also reserves a top identity/status band
so boot diagnostics scroll in a cleaner text area.

The graphical runner also exposes a QEMU virtio keyboard device. When the QEMU
framebuffer window has focus, printable keys are delivered to the shell through
the graphical input path. UART input remains available through the terminal.

## Next Milestone

The next graphics milestone is improving the framebuffer console presentation:

- keep the QEMU-visible framebuffer stable
- add a shell status line
