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
kernel configures a 640x480 XRGB8888 framebuffer, draws a simple pixel test
pattern, and renders early bitmap text before starting the scheduler.

## Next Milestone

The next graphics milestone is routing the kernel console to the framebuffer:

- keep the QEMU-visible framebuffer stable
- route console text to both UART and framebuffer
- add scrolling for framebuffer console output
