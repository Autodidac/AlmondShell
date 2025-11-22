# Context Module Audit

The following table summarises the current state of every file whose name includes "context".  "Earmark" indicates whether the file should be queued for removal in a future cleanup pass.

| Path | Status | Notes | Earmark |
| --- | --- | --- | --- |
| `include/acontext.hpp` | Active | Core context abstraction referenced throughout the engine. | No |
| `include/acontextcontrol.hpp` | Active | Provides command helpers for draw operations; required by the multiplexer. | No |
| `include/acontextmultiplexer.hpp` | Active | Coordinates backend switching; heavily used by `src/acontextmultiplexer.cpp`. | No |
| `src/acontextmultiplexer.cpp` | Active | Implements multiplexer runtime; necessary for desktop builds. | No |
| `include/acontextrenderer.hpp` | Legacy Stub | Mostly commented-out scaffolding with alternate renderer hooks; not referenced at runtime. | Maybe |
| `include/acontextstate.hpp` | Active | Shares renderer state structs across backends; used by Raylib helpers. | No |
| `include/acontexttype.hpp` | Active | Enumerations for backend selection; consumed by virtually every renderer. | No |
| `include/acontextwindow.hpp` | Active | Wraps native window handles; needed by all backends. | No |
| `include/aopenglcontext.hpp` | Active | Fully implemented OpenGL backend. | No |
| `include/asfmlcontext.hpp` | Active | Used by the SFML backend wiring in `src/acontext.cpp`. | No |
| `include/asdlcontext.hpp` | Active | Required for the SDL backend and renderer integration. | No |
| `include/asoftrenderer_context.hpp` | Active | Powers the software renderer; still under development but exercised by tooling. | No |
| `include/anoopcontext.hpp` | Minimal Stub | Only compiled for headless builds; harmless placeholder for tests. | No |
| `include/araylibcontext.hpp` | Active | Primary Raylib backend implementation. | No |
| `include/araylibcontextinput.hpp` | Active | Required to translate Raylib input to engine enums. | No |
| `include/araylibstate.hpp` | Active | Shared Raylib state container; consumed by renderer and context glue. | No |
| `include/asdlcontextrenderer.hpp` | Partial | SDL renderer wrapper is wired in but lacks texture batching; revisit once SDL backend stabilises. | No |
| `src/acontext.cpp` | Active | Registers every backend and owns the dispatch table. | No |

Continue reviewing entries marked "Maybe" before performing destructive changes; the prior "Yes" candidates have been excised from the tree.
