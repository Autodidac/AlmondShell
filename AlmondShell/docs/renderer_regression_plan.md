# Renderer Regression Plan

This plan defines the smoke coverage, capture workflow, and acceptance criteria
for validating AlmondShell's renderer backends (OpenGL, SDL, Raylib, software).
It also identifies telemetry hooks required to make resize and framebuffer
behaviour observable when the plan is executed.

## Menu overlay invariants

Menu smoke scenes exercise the menu overlay; keep the following invariants in
mind when auditing backend behaviour:

1. `MenuOverlay::recompute_layout` caches `ctx->get_width_safe()` and
   `ctx->get_height_safe()` and centres the grid from those dimensions, so the
   context getters must track the actual pixel space that `draw_sprite_safe`
   renders into.【F:AlmondShell/include/aguimenu.hpp†L101-L170】
2. The cached positions produced during layout are reused for both hit-testing
   and the GUI cursor placement before each `gui::button` call in
   `update_and_draw`, so backend scaling still has to keep layout coordinates
   aligned with the GUI draw pass.【F:AlmondShell/include/aguimenu.hpp†L277-L395】
3. `update_and_draw` compares `ctx->get_mouse_position_safe()` against the same
   rectangles it submits to `draw_sprite_safe`, so pointer coordinates have to
   arrive already translated into the sprite grid (with the OpenGL flip handled
   explicitly in the overlay).【F:AlmondShell/include/aguimenu.hpp†L285-L352】【F:AlmondShell/include/aguimenu.hpp†L374-L395】

## 1. Smoke scenes per backend

| Backend | Launch arguments / setup | Scene coverage | Expected outputs |
|---------|-------------------------|----------------|------------------|
| OpenGL | `--renderer=opengl --scene=dockstress` (multi-window default) | *Context bring-up:* Verify Win32 child creation and GL context binding.<br>*Dock grid:* Spawn four docked panes to validate shared context usage.<br>*Sprite atlas:* Trigger `atlasmanager::register_backend_uploader` uploads to exercise texture residency. | Stable 4-pane layout, consistent GL viewport sizes, animated quad sampler with no shader recompilation errors. |
| SDL | `--renderer=sdl --scene=menu_rainbow` | *Renderer output sizing:* `SDL_GetCurrentRenderOutputSize` should track Win32 client changes.<br>*Event pump:* Escape key exits, resize storm drained without blocking queue.<br>*Atlas upload:* SDL uploader flushes textures. | Background hue cycling without hitching, resize events leave canvas centred, command queue drains to empty each frame. |
| Raylib | `--renderer=raylib --scene=fit_canvas` | *Framebuffer scaling:* `dispatch_resize` coalesces OS + virtual canvas updates.<br>*Dock detach/reattach:* Exercise `MakeDockable` parent switching.<br>*Child-window capture:* Dock the pane, record parent/child dimensions, and verify the GUI retains the design canvas while the Raylib GL context stays current.<br>*Input scale:* Pointer offset matches viewport transform. | Render canvas letterboxed with `GuiFitViewport` scale, framebuffer + logical sizes reported, pointer focus stays aligned after resize/dock, context logs never report a lost binding. |
| Raylib (GL disabled) | Build with `ALMOND_USING_OPENGL` undefined, then launch `./Bin/GCC-Debug/cmakeapp1/cmakeapp1 --renderer=raylib --scene=fit_canvas` | *Context bootstrap without shared OpenGL renderer:* Verify Win32 handle adoption after Raylib re-parents into the dock host.<br>*Smoke:* Toggle dock attach/detach once after startup. | GUI letterboxes correctly, logs contain no "Render context became unavailable" warnings, viewport and input scaling remain stable. |
| Software | `--renderer=software --scene=cpu_quad` | *Framebuffer growth:* `resize_framebuffer` expands pixel buffer.<br>*Command queue:* Ensures queued resizes don't recurse.<br>*Atlas rebuild:* CPU atlas regeneration path used for missing GPU uploads. | CPU renderer updates checkerboard quad without tearing, resize callback fires once per logical size, framebuffer buffer length matches width×height. |

Each scene should run for 60 seconds with window resizes at 0s, 20s, and 40s
(simulate drag to <50% and >150% of base size). Dock/undock actions should be
performed for OpenGL and Raylib immediately after the first resize.

## 2. Capture & tooling workflow

1. **Automated launch harness** – extend the existing smoke harness to accept a
   `--capture` flag. When set, spawn the renderer with deterministic window sizes
   (1280×720 baseline) before scripted resizes and dock actions.
2. **Frame capture** –
   - OpenGL: trigger RenderDoc capture on the second frame after each resize.
   - SDL: capture via OBS recording of window region; use SDL renderer stats for
     frame pacing.
   - Raylib: use built-in `TakeScreenshot` after each resize to archive canvas
     output.
   - Software: dump framebuffer to PNG via `SoftwareRenderer::present` hook.
3. **Log collection** – pipe stdout/stderr to `Logs/<backend>/YYYYMMDD-hhmm.log`.
   Include telemetry counters (see §3) and queue depth snapshots.
4. **Artifact review** – add a checklist to the QA portal referencing these
   captures with links and pass/fail toggles per backend run.

### Menu overlay GUI bounds trace checklist

Menu smoke scenes now emit the GUI-computed bounds for menu button 0 when
launched with `--trace-menu-button0`. Capture the log line below for each backend
as part of the smoke workflow to verify that layout, GUI drawing, and input all
share the same coordinate space after the refactor.

| Backend | Command (from `AlmondShell` root after building) | Expected log snippet |
|---------|--------------------------------------------------|----------------------|
| OpenGL | `./Bin/GCC-Debug/cmakeapp1/cmakeapp1 --renderer=opengl --scene=dockstress --trace-menu-button0` | `[MenuOverlay] button0 GUI bounds: x=272.00 y=632.00 w=320.00 h=120.00 (OpenGL input flip)` |
| SDL | `./Bin/GCC-Debug/cmakeapp1/cmakeapp1 --renderer=sdl --scene=menu_rainbow --trace-menu-button0` | `[MenuOverlay] button0 GUI bounds: x=272.00 y=328.00 w=320.00 h=120.00` |
| Raylib | `./Bin/GCC-Debug/cmakeapp1/cmakeapp1 --renderer=raylib --scene=fit_canvas --trace-menu-button0` | `[MenuOverlay] button0 GUI bounds: x=272.00 y=328.00 w=320.00 h=120.00` |
| Software | `./Bin/GCC-Debug/cmakeapp1/cmakeapp1 --renderer=software --scene=cpu_quad --trace-menu-button0` | `[MenuOverlay] button0 GUI bounds: x=272.00 y=328.00 w=320.00 h=120.00` |

Archive the stdout snippet alongside the per-backend screenshots so future
regression runs can diff the coordinates without rerunning the scenes.

## 3. Telemetry instrumentation

| Signal | Description | Integration points |
|--------|-------------|--------------------|
| `renderer.resize.count` | Increment each time the multiplexer coalesces a resize. | `MultiContextManager::HandleResize` after the window/context width/height write; hook when enqueueing the render-thread callback so counts cover client-visible events.【F:AlmondShell/src/acontextmultiplexer.cpp†L780-L815】 |
| `renderer.resize.latest_dimensions` | Gauges for last width/height per window and backend. | Populate immediately after `window->context->width/height` are updated in `HandleResize`, tagging with `ContextType`.【F:AlmondShell/src/acontextmultiplexer.cpp†L780-L812】 |
| `renderer.framebuffer.size` | Record backend framebuffer dimensions after internal adjustments. |
- OpenGL: sample `glViewport` dimensions right after `glViewport(0, 0, ctx->width, ctx->height)` inside the init path and any resize handler.【F:AlmondShell/include/aopenglcontext.hpp†L416-L430】
- SDL: capture `renderW/renderH` from `SDL_GetCurrentRenderOutputSize` before presenting.【F:AlmondShell/include/asdlcontext.hpp†L254-L302】
- Raylib: emit `safeFbW/safeFbH` plus logical canvas values within `dispatch_resize` after coalescing.【F:AlmondShell/include/araylibcontext.hpp†L101-L170】
- Software: push `sr.width/sr.height` once `resize_framebuffer` completes and include buffer length for validation.【F:AlmondShell/include/asoftrenderer_context.hpp†L41-L123】 |
| `renderer.command_queue.depth` | Sample queued command count before drain to ensure storms are handled. | Report `commandQueue.size()` before `queue.drain()` in backend process loops (`sdl_process`, Raylib present hook, software present) and inside `RenderLoop` before `process_safe` execution.【F:AlmondShell/include/asdlcontext.hpp†L274-L306】【F:AlmondShell/src/acontextmultiplexer.cpp†L996-L1065】 |
| `renderer.frame.time_ms` | Frame duration histogram per backend for the smoke runs. | Stamp `RobustTime` timers around backend-present paths (`sdl_process` rainbow loop, Raylib `BeginDrawing`/`EndDrawing`, software blit) and aggregate in telemetry buffers. For OpenGL reuse the existing quad pipeline draw to flush timings.【F:AlmondShell/include/asdlcontext.hpp†L267-L312】【F:AlmondShell/include/araylibcontext.hpp†L59-L170】【F:AlmondShell/include/asoftrenderer_context.hpp†L93-L168】【F:AlmondShell/include/aopenglcontext.hpp†L288-L430】 |

### Hook sequencing notes

- Emit telemetry inside the render-thread contexts after they adopt the new
  size to avoid races with UI threads; leverage the existing `window->commandQueue`
  to funnel counter updates alongside client callbacks.【F:AlmondShell/src/acontextmultiplexer.cpp†L780-L815】
- Backend-specific gauges should include window identifiers (`HWND` pointer or
  multiplexer index) so docked panes can be correlated.
- Integrate telemetry flush with the existing atlas upload checkpoints to ensure
  metrics are captured even when frames short-circuit (e.g. SDL escape exit).

## 4. Pass/fail criteria

A backend smoke run passes when:

1. **Startup** – Window/context initialises without errors; telemetry confirms a
   single resize at startup (baseline sizing) followed by three scripted resizes.
2. **Resize fidelity** – Framebuffer telemetry matches window size (+expected
   virtual canvas for Raylib) within ±1 pixel after each resize.
3. **Command queue health** – Queue depth returns to zero within two frames of
   each scripted resize/dock action.
4. **Visual output** – Captured screenshots/video show expected scene without
   artefacts (colour cycling, checkerboard integrity, viewport fit).
5. **Stability** – Render loop runs 60 seconds without thread panics; frame time
   telemetry stays below 16.7ms average for hardware backends and 33.3ms for the
   software renderer.

Failures should capture offending telemetry payloads and link to their
associated logs/captures for triage.

## 5. Future automation hooks

- Expose a `RendererTelemetrySink` interface so CI smoke jobs can upload metrics
  to the dashboard without running the full editor tooling.
- Extend the multiplexer’s shutdown path to emit final counters (total frames,
  total resizes) for comparison across runs.
- Add CLI switches for backend-specific instrumentation (e.g. forcing Raylib to
  log logical vs framebuffer dimensions every frame when `--trace-raylib-resize`
  is provided).
