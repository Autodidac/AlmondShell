
<img align="left" src="AlmondShell/images/567.jpg" width="70px"/>


# AlmondShell



AlmondShell combines a hot-reloadable C++ engine with a self-updating launcher. the engine runtime drives editor scripts from `src/scripts/`.

- **End users** can download the prebuilt binary, place it in an empty directory, and let it populate the latest AlmondShell files automatically.

---

## Key Features

- 🔄 **Self-updating launcher** that fetches the newest release when run.
- ⚙️ **Modular C++20 engine** with systems for rendering, scripting, tasks, and asset management.
- 🧪 **Live script reloading** – changes to `*.ascript.cpp` files are detected at runtime and recompiled automatically.
- 🗂️ **Well-organised codebase** with headers in `include/`, implementation in `src/`, and helper scripts under `unix/` and project-level `.sh` helpers.

---

## Repository Layout

```
.
├── README.md                # Project overview (this file)
├── AlmondShell/
│   ├── include/             # Core engine headers
│   ├── src/                 # Engine, updater entry point, and scripts
│   ├── docs/                # Supplementary documentation and setup notes
│   ├── examples/            # Sample projects and templates
│   └── CMakeLists.txt       # Build script for the updater target
└── AlmondShell.sln          # Visual Studio solution for Windows developers
```

Refer to `AlmondShell/docs/file_structure.txt` for a more exhaustive tour of the available modules.

---

## Prerequisites

To build AlmondShell from source you will need the following tools:

| Requirement            | Notes |
| ---------------------- | ----- |
| A C++20 toolchain      | Visual Studio 2022, clang, or GCC 11+ are recommended. |
| CMake ≥ 3.10           | Used to generate build files. |
| Ninja _or_ MSBuild     | Pick the generator that matches your platform. |
| Git                    | Required for cloning the repository and fetching dependencies. |
| [vcpkg](https://vcpkg.io/) | Simplifies acquiring third-party libraries such as `asio`. |
| Optional: Vulkan SDK   | Needed when working on Vulkan backends listed in `include/avulkan*`. |

> **Note:** The project expects the header-only [`asio`](https://think-async.com/) library. When using vcpkg run `vcpkg install asio` (or add it to your manifest) before configuring the build.

---

## Building from Source

Clone the repository and configure the build with your preferred toolchain:

```bash
git clone https://github.com/Autodidac/AlmondShell.git
cd AlmondShell
```

### Windows (MSVC)

```powershell
cmake -B build -S . -G "Visual Studio 17 2022"
cmake --build build --config Release
```

### Linux & macOS (Clang/GCC + Ninja)

```bash
cmake -B build -S . -G Ninja
cmake --build build
```

Both configurations produce the `updater` executable inside the `build` directory. Adjust `--config` or the generator settings to create Debug builds when needed.

---

## Running the Updater & Engine

### Using a Release Binary
1. Create an empty working directory.
2. Download the appropriate `almondshell` binary from the [Releases](https://github.com/Autodidac/AlmondShell/releases) page.

### Running From Source

On launch the updater:
1. Reads the remote configuration targets defined in `include/aupdateconfig.hpp` (for example the `include/config.hpp` manifest in the release repository).
2. Downloads and applies updates when available.
3. Starts the engine runtime, which in turn loads `src/scripts/editor_launcher.ascript.cpp` and watches for changes. Editing the script triggers automatic recompilation within the running session.

Stop the session with `Ctrl+C` or by closing the console window.

---

## Development Tips

- The hot-reload loop in `src/main.cpp` monitors script timestamps roughly every 200 ms. Keep editor builds incremental to benefit from the fast feedback.
- Utility shell scripts (`build.sh`, `run.sh`, `unix/*.sh`) can streamline development on POSIX systems.
- Check the `docs/` folder for platform-specific setup guides, tool recommendations, and dependency notes.

---

## Contributing

"We Are Not Accepting PRs At This Time" as Almond is a source available commercial product

For substantial changes, open an issue first to discuss direction.

---

## License

The project uses the `LicenseRef-MIT-NoSell` license variant. See [`LICENSE`](LICENSE) for the full terms, including restrictions on commercial use and warranty disclaimers.


