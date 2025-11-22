# AlmondShell v0.70.0 Release Notes

## Highlights
- Fixes a segmentation fault when the Software backend spins up alongside Raylib on Linux by wiring the software renderer into the X11 multiplexer initialisation.
- Registers the software renderer before render threads launch so resize callbacks, atlas uploads, and framebuffer allocation succeed from the first frame.

## Known Issues
- The software renderer still renders to an in-memory framebuffer on Linux; presenting to a native window remains a future enhancement.
- Vulkan and DirectX backends are stubbed out on non-Windows platforms.

## Upgrade Notes
- Rebuild the project after pulling to pick up the Linux multiplexer changes and the new version number.
