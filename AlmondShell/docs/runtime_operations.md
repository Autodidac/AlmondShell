# Runtime Operations Guide

## Prerequisites
- Start from the launcher entry point (`RunEngine`) so the updater and scripting hooks are available inside the runtime loop before you launch any contexts.【F:AlmondShell/examples/ConsoleApplication1/main.cpp†L39-L107】
- Provision a `ScriptScheduler` instance before enabling hot reload; the scheduler drives compilation tasks and executes the reloaded entry point.【F:AlmondShell/examples/ConsoleApplication1/main.cpp†L68-L107】【F:AlmondShell/src/ascriptingsystem.cpp†L159-L172】
- Keep your script sources under `src/scripts/` so the loader can resolve `<name>.ascript.cpp` inputs and produce matching DLLs in the same directory.【F:AlmondShell/src/ascriptingsystem.cpp†L61-L125】

## Update Flow
- The launcher’s first stage is an updater check that can clear stale artifacts, query the hosted manifest, and pull down a fresh build when a new version is available.【F:AlmondShell/examples/ConsoleApplication1/main.cpp†L39-L62】
- When no update is required the routine falls back to a clean console state and continues to engine start-up, keeping operator feedback consistent between refresh cycles.【F:AlmondShell/examples/ConsoleApplication1/main.cpp†L55-L61】
- Re-run the launcher with `--update --force` to apply the update immediately: this path downloads the matching source archive, expands it beside the executable, provisions the toolchain helpers, and rebuilds the binary so the recovered repository tree is available for inspection or local modifications.【F:AlmondShell/include/aupdatesystem.hpp†L325-L408】【F:AlmondShell/include/aupdatesystem.hpp†L474-L499】

## Hot-Reload Cycle
- The sample runtime demonstrates polling the script timestamp roughly every 200 ms and calling `load_or_reload_script` whenever the file changes, keeping edit–compile–run latency tight.【F:AlmondShell/examples/ConsoleApplication1/main.cpp†L84-L104】
- `load_or_reload_script` schedules a coroutine that compiles the updated source into a DLL, unloads any previously mapped handle, and executes the exported `run_script` symbol once the new library is resident.【F:AlmondShell/src/ascriptingsystem.cpp†L61-L141】
- The scheduler waits for all tasks to finish and prunes completed nodes so your tooling observes a deterministic runtime state after each reload.【F:AlmondShell/src/ascriptingsystem.cpp†L165-L172】

## Multi-Context Troubleshooting
- Releasing the previous library handle (`FreeLibrary`/`dlclose`) before loading the replacement prevents Windows and POSIX backends from pinning stale code when multiple contexts request the same script in quick succession.【F:AlmondShell/src/ascriptingsystem.cpp†L76-L125】
- Windows builds that embed alternate front ends (SDL, Raylib) may route through dedicated entry points; ensure headless overrides are disabled when you expect the shared `RunEngine` path to initialise every context.【F:AlmondShell/examples/ConsoleApplication1/main.cpp†L39-L107】【F:AlmondShell/include/aengineconfig.hpp†L26-L35】
- When juggling several renderers, confirm each required backend macro is enabled so the scheduler can safely execute per-context script logic after reloads.【F:AlmondShell/include/aengineconfig.hpp†L53-L70】

## Adjusting `aengineconfig.hpp`
- Toggle `ALMOND_SINGLE_PARENT` to switch between a single parent window with children and fully independent top-level windows during multi-context debugging.【F:AlmondShell/include/aengineconfig.hpp†L53-L55】
- Enable or disable specific context integrations (`ALMOND_USING_SDL`, `ALMOND_USING_RAYLIB`, etc.) to replicate your live configuration in development environments before testing hot reload interactions.【F:AlmondShell/include/aengineconfig.hpp†L57-L66】
- If you rely on Raylib, keep the provided Windows header guards and symbol remaps so the build remains stable across toolchains; they are required for successful reloads on Win32 and should only be altered with care.【F:AlmondShell/include/aengineconfig.hpp†L89-L197】

## Interpreting `ScriptLoadReport`
- `ScriptLoadReport::reset` clears stage flags and previous log entries whenever a reload begins, so inspect the latest run before triggering another iteration.【F:AlmondShell/include/ascriptingsystem.hpp†L41-L57】
- A failed compilation sets `failed=true` and records the diagnostic message, while successful progression flips each stage flag (`scheduled`, `compiled`, `dllLoaded`, `executed`) so you can pinpoint where the pipeline stalled.【F:AlmondShell/include/ascriptingsystem.hpp†L60-L90】【F:AlmondShell/src/ascriptingsystem.cpp†L61-L141】
- Use `messages()` to capture the accumulated info/error entries for your editor UI or logging sink when presenting reload results to contributors.【F:AlmondShell/include/ascriptingsystem.hpp†L72-L85】

## Handling Reload Failures
- Missing sources, compilation errors, loader failures, and absent entry symbols each emit a targeted error message. Surface these to the developer so they can correct the underlying issue without guessing the failing stage.【F:AlmondShell/src/ascriptingsystem.cpp†L69-L140】
- Scheduling exceptions are caught at the outer entry point, logged, and flagged in the report to prevent silent background failures when the task graph cannot start the reload job.【F:AlmondShell/src/ascriptingsystem.cpp†L159-L179】
