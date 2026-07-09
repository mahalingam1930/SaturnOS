# SaturnOS Graphics

## Current Status

SaturnOS currently renders kernel output through the PL011 UART serial console.
The default runner uses QEMU's `-nographic` mode, so it intentionally does not
open a display window.

For a graphical QEMU window with the UART console attached to the window, run:

```sh
./scripts/run-gui.sh
```

At this stage, the QEMU window is a graphical serial console. SaturnOS does not
yet include a framebuffer or GPU driver, so kernel text is still produced
through UART even though QEMU can display that serial output in a window.

## Next Milestone

The next graphics milestone is a framebuffer console:

- discover or configure a QEMU-visible framebuffer
- map the framebuffer into the kernel
- draw pixels and clear the screen
- add a tiny bitmap font
- route console text to both UART and framebuffer
