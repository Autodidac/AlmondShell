# Menu overlay backend audit

This checklist records where each backend wires menu-sensitive primitives and notes any
mismatches relative to the software renderer baseline.

## Software (baseline)
- [x] Window events keep `ctx->width/height` aligned with the Win32 client size and
      the software framebuffer via the multiplexer and `resize_framebuffer` guard.【F:AlmondShell/src/acontextmultiplexer.cpp†L720-L812】【F:AlmondShell/include/asoftrenderer_context.hpp†L64-L160】
- [x] `get_width/height` forward to the software framebuffer dimensions used by
      `draw_sprite`, so layout queries and rendering share the same pixel grid.【F:AlmondShell/include/asoftrenderer_context.hpp†L360-L409】
- [x] `draw_sprite` consumes pixel-space coordinates without additional scaling,
      matching the cached menu positions.【F:AlmondShell/include/asoftrenderer_context.hpp†L239-L339】
- [x] Mouse polling keeps coordinates global and lets `get_mouse_position_safe`
      project them into the window client space used for hit-testing.【F:AlmondShell/include/ainput.hpp†L300-L320】
- ✅ No parity issues observed; this is the reference behaviour.

## OpenGL
- [x] `opengl_initialize` seeds both the context and GL state with the window
      client size, and runtime resizes propagate through the multiplexer.
      【F:AlmondShell/include/aopenglcontext.hpp†L294-L356】【F:AlmondShell/src/acontextmultiplexer.cpp†L720-L812】
- [x] `opengl_get_width/height` read back the current viewport dimensions so
      layout queries track the drawable surface.【F:AlmondShell/include/aopenglcontext.hpp†L400-L419】
- [x] The OpenGL `draw_sprite` path resolves the live viewport before converting
      menu coordinates into clip-space quads.【F:AlmondShell/include/aopengltextures.hpp†L332-L420】
- [x] Mouse input reuses the global polling path and the overlay’s explicit
      vertical flip keeps hit-testing consistent.【F:AlmondShell/src/acontext.cpp†L431-L468】【F:AlmondShell/include/aguimenu.hpp†L298-L343】
- ✅ Behaviour matches the software baseline.

## SDL
- [x] SDL’s initialization and resize callback keep `ctx->width/height` in sync
      with the renderer output size collected each frame.【F:AlmondShell/include/asdlcontext.hpp†L72-L158】【F:AlmondShell/include/asdlcontext.hpp†L287-L348】
- [x] `sdl_get_width/height` query the renderer output (falling back to the last
      known window size) so layout sees the same dimensions as the blitter.
      【F:AlmondShell/include/asdlcontext.hpp†L449-L472】
- [x] `draw_sprite` maps menu coordinates through the active renderer viewport
      without inventing extra scaling rules.【F:AlmondShell/include/asdltextures.hpp†L241-L352】
- [x] Mouse input piggybacks on the shared Windows polling path, yielding client
      coordinates compatible with the cached menu rectangles.【F:AlmondShell/src/acontext.cpp†L471-L507】【F:AlmondShell/include/ainput.hpp†L300-L320】
- ✅ Behaviour matches the software baseline.

## Raylib
- [x] `dispatch_resize` forces `ctx->width/height` to the CLI “virtual” canvas
      while tracking the physical framebuffer separately.【F:AlmondShell/include/araylibcontext.hpp†L160-L218】
- [x] `raylib_get_width/height` currently return the logical window reported by
      Raylib (`GetScreenWidth/Height`).【F:AlmondShell/include/araylibcontext.hpp†L503-L507】
- [x] The Raylib `draw_sprite` implementation expects menu coordinates in the
      virtual canvas and scales them into the fitted viewport.【F:AlmondShell/include/araylibrenderer.hpp†L120-L188】【F:AlmondShell/include/araylibcontext.hpp†L411-L430】
- [x] Mouse polling switches to Raylib’s per-frame cursor data and disables the
      global-to-client conversion after applying `SetMouseOffset/Scale`.【F:AlmondShell/include/araylibcontext.hpp†L411-L430】【F:AlmondShell/include/araylibcontextinput.hpp†L105-L132】
- [x] Viewport seeding now reads the live framebuffer during context creation so
      atlas-driven overlays stay aligned before the child window finishes
      docking.【F:AlmondShell/include/araylibcontext.hpp†L79-L189】【F:AlmondShell/include/araylibcontext.hpp†L318-L429】
- [x] Align `ctx->get_width_safe()/get_height_safe()` with the virtual canvas so
      menu layout positions match the viewport scaling used during draw calls.【F:AlmondShell/include/araylibcontext.hpp†L160-L218】【F:AlmondShell/include/araylibcontext.hpp†L503-L507】
- [x] Confirm that hit-testing uses the same scaled coordinates Raylib hands back
      after `SetMouseScale` to avoid menu hover mismatches.【F:AlmondShell/include/araylibcontext.hpp†L411-L430】【F:AlmondShell/include/araylibcontextinput.hpp†L105-L132】

Verification: Launch `--renderer=raylib --scene=fit_canvas --trace-menu-button0`, dock the pane, and confirm the logged GUI bounds for button 0 match the design-canvas dimensions reported by the immediate-mode layer while pointer hover events trigger without offset after resizing and context reacquisition.

