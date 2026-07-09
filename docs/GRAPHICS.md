# SaturnOS Graphics

## Current Status

SaturnOS currently renders kernel output through the PL011 UART serial console.
The default runner uses QEMU's `-nographic` mode, so it intentionally does not
open a display window.

For a graphical QEMU window with the UART console still attached to the
terminal, run:

```sh
./scripts/run-gui.sh
```

At this stage, the QEMU window is only the display surface. SaturnOS does not
yet include a framebuffer or GPU driver, so kernel text still appears through
UART.

## Next Milestone

The next graphics milestone is a framebuffer console:

- discover or configure a QEMU-visible framebuffer
- map the framebuffer into the kernel
- draw pixels and clear the screen
- add a tiny bitmap font
- route console text to both UART and framebuffer
