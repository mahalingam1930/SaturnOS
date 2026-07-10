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
kernel configures a 640x480 XRGB8888 framebuffer and draws a simple pixel test
pattern before starting the scheduler.

## Next Milestone

The next graphics milestone is a framebuffer console:

- keep the QEMU-visible framebuffer stable
- add a tiny bitmap font
- route console text to both UART and framebuffer
