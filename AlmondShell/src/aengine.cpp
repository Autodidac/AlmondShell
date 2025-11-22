/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   This file is part of the Almond Project.                 *
 *   AlmondShell - Modular C++ Framework                      *
 *                                                            *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
 *                                                            *
 *   Provided "AS IS", without warranty of any kind.          *
 *   Use permitted for Non-Commercial Purposes ONLY,          *
 *   without prior commercial licensing agreement.            *
 *                                                            *
 *   Redistribution Allowed with This Notice and              *
 *   LICENSE file. No obligation to disclose modifications.   *
 *                                                            *
 *   See LICENSE file for full terms.                         *
 *                                                            *
 **************************************************************/
// aengine.cpp
module AlmondShell.aengine;

import std;
import AlmondShell.aengine:platform;
import AlmondShell.aengine:engine_components;
import AlmondShell.aengine:renderers;

//#if !defined(ALMOND_SINGLE_PARENT) || (ALMOND_SINGLE_PARENT == 0)
//#undef ALMOND_SHARED_CONTEXT
//#define ALMOND_SHARED_CONTEXT 0
//#else
//#define ALMOND_USING_DOCKING 1  // Enable docking features
//#define ALMOND_SHARED_CONTEXT 1
//#endif

namespace cli = almondnamespace::core::cli;

namespace almondnamespace::core
{
    class MultiContextManager; // Forward declaration
    //std::shared_ptr<core::Context> MultiContextManager::currentContext = nullptr;
	//std::shared_ptr<core::Context> MultiContextManager::sharedContext = nullptr;
    
    // ─── 1) Declare a global ContextPtr somewhere visible to WinMain & WndProc ───
   // inline std::shared_ptr<almondnamespace::Context> g_ctx;

    //std::filesystem::path myexepath;

    //void printEngineInfo() {
    //    std::cout << almondnamespace::GetEngineName() << " v" 
    //              << almondnamespace::GetEngineVersion() << '\n';
    //}

    //void handleCommandLine(int argc, char* argv[]) {
    //    std::cout << "Command-line arguments for: ";
    //    for (int i = 0; i < argc; ++i) {
    //        if (i > 0) {
    //            std::cout << "  [" << i << "]: " << argv[i] << '\n';
    //        }
    //        else {
    //            myexepath = argv[i];
    //            std::string filename = myexepath.filename().string();
    //            std::cout << filename << '\n';
    //        }
    //    }
    //    std::cout << '\n'; // End arguments with a new line before printing anything else

    //    // Handle specific commands
    //    for (int i = 1; i < argc; ++i) {
    //        std::string arg = argv[i];

    //        if (arg == "--help" || arg == "-h") {
    //            std::cout << "Available commands:\n"
    //                << "  --help, -h            Show this help message\n"
    //                << "  --version, -v         Display the engine version\n"
    //                << "  --fullscreen          Run the engine in fullscreen mode\n"
    //                << "  --windowed            Run the engine in windowed mode\n"
    //                << "  --width [value]       Set the window width (e.g., --width 1920)\n"
    //                << "  --height [value]      Set the window height (e.g., --height 1080)\n";
    //        }
    //        else if (arg == "--version" || arg == "-v") {
    //            printEngineInfo();
    //        }
    //        else if (arg == "--fullscreen") {
    //            std::cout << "Running in fullscreen mode.\n";
    //            // Set fullscreen mode flag
    //        }
    //        else if (arg == "--windowed") {
    //            std::cout << "Running in windowed mode.\n";
    //            // Set windowed mode flag
    //        }
    //        else if (arg == "--width" && i + 1 < argc) {
    //            almondnamespace::core::cli::window_width = std::stoi(argv[++i]);
    //            std::cout << "Window width set to: " << almondnamespace::core::cli::window_width << '\n';
    //        }
    //        else if (arg == "--height" && i + 1 < argc) {
    //            almondnamespace::core::cli::window_height = std::stoi(argv[++i]);
    //            std::cout << "Window height set to: " << almondnamespace::core::cli::window_height << '\n';
    //        }
    //        else {
    //            std::cerr << "Unknown argument: " << arg << '\n';
    //        }
    //    }
    //}

    namespace
    {
        template <typename PumpFunc>
        int RunEngineMainLoopCommon(almondnamespace::core::MultiContextManager& mgr, PumpFunc&& pump_events)
        {
            enum class SceneID {
                Menu,
                Snake,
                Tetris,
                Pacman,
                Sokoban,
                Match3,
                Sliding,
                Minesweeper,
                Game2048,
                Sandsim,
                Cellular,
                Exit
            };

            SceneID g_sceneID = SceneID::Menu;
            std::unique_ptr<almondnamespace::scene::Scene> g_activeScene;
            almondnamespace::menu::MenuOverlay menu;
            menu.set_max_columns(cli::menu_columns);

            auto collect_backend_contexts = []()
            {
                using ContextGroup = std::pair<almondnamespace::core::ContextType,
                    std::vector<std::shared_ptr<almondnamespace::core::Context>>>;
                std::vector<ContextGroup> snapshot;
                {
                    std::shared_lock lock(almondnamespace::core::g_backendsMutex);
                    snapshot.reserve(almondnamespace::core::g_backends.size());
                    for (auto& [type, state] : almondnamespace::core::g_backends) {
                        std::vector<std::shared_ptr<almondnamespace::core::Context>> contexts;
                        contexts.reserve(1 + state.duplicates.size());
                        if (state.master)
                            contexts.push_back(state.master);
                        for (auto& dup : state.duplicates)
                            contexts.push_back(dup);
                        snapshot.emplace_back(type, std::move(contexts));
                    }
                }
                return snapshot;
            };

            auto init_menu = [&]() {
                auto backendContexts = collect_backend_contexts();
                for (auto& [_, contexts] : backendContexts) {
                    for (auto& ctx : contexts) {
                        if (ctx) menu.initialize(ctx);
                    }
                }
            };
            init_menu();

            std::unordered_map<almondnamespace::core::Context*, std::chrono::steady_clock::time_point> lastFrameTimes;

            bool running = true;

            while (running) {
                if (!pump_events()) {
                    running = false;
                    break;
                }

                auto backendContexts = collect_backend_contexts();
                for (auto& [type, contexts] : backendContexts) {
                    auto update_on_ctx = [&](std::shared_ptr<almondnamespace::core::Context> ctx) -> bool {
                        if (!ctx) return true;
                        auto* win = mgr.findWindowByHWND(ctx->hwnd);
                        if (!win)
                            win = mgr.findWindowByContext(ctx);
                        if (!win)
                            return true; // window not ready yet

                        bool ctxRunning = win->running;

                        const auto now = std::chrono::steady_clock::now();
                        const auto rawCtx = ctx.get();
                        float dtSeconds = 0.0f;
                        auto [timeIt, inserted] = lastFrameTimes.emplace(rawCtx, now);
                        if (!inserted) {
                            dtSeconds = std::chrono::duration<float>(now - timeIt->second).count();
                            timeIt->second = now;
                        }

                        auto begin_scene = [&](auto makeScene, SceneID id) {
                            auto clear_commands = [](const std::shared_ptr<almondnamespace::core::Context>& context) {
                                if (context && context->windowData) {
                                    context->windowData->commandQueue.clear();
                                }
                            };

                            auto snapshot = collect_backend_contexts();
                            for (auto& [_, group] : snapshot) {
                                for (auto& contextPtr : group) {
                                    clear_commands(contextPtr);
                                }
                            }

                            menu.cleanup();
                            if (g_activeScene) {
                                g_activeScene->unload();
                            }
                            g_activeScene = makeScene();
                            g_activeScene->load();
                            g_sceneID = id;
                        };

                        switch (g_sceneID) {
                        case SceneID::Menu: {
                            int mx = 0, my = 0;
                            ctx->get_mouse_position_safe(mx, my);
                            const almondnamespace::gui::Vec2 mousePos{
                                static_cast<float>(mx),
                                static_cast<float>(my)
                            };
                            const bool mouseLeftDown = almondnamespace::input::mouseDown.test(almondnamespace::input::MouseButton::MouseLeft);
                            const bool upPressed = almondnamespace::input::keyPressed.test(almondnamespace::input::Key::Up);
                            const bool downPressed = almondnamespace::input::keyPressed.test(almondnamespace::input::Key::Down);
                            const bool leftPressed = almondnamespace::input::keyPressed.test(almondnamespace::input::Key::Left);
                            const bool rightPressed = almondnamespace::input::keyPressed.test(almondnamespace::input::Key::Right);
                            const bool enterPressed = almondnamespace::input::keyPressed.test(almondnamespace::input::Key::Enter);

                            almondnamespace::gui::begin_frame(ctx, dtSeconds, mousePos, mouseLeftDown);
                            auto choice = menu.update_and_draw(ctx, win, dtSeconds, upPressed, downPressed, leftPressed, rightPressed, enterPressed);
                            almondnamespace::gui::end_frame();

                            if (choice) {
                                if (*choice == almondnamespace::menu::Choice::Snake) {
                                    begin_scene([] { return std::make_unique<almondnamespace::snake::SnakeScene>(); }, SceneID::Snake);
                                }
                                else if (*choice == almondnamespace::menu::Choice::Tetris) {
                                    begin_scene([] { return std::make_unique<almondnamespace::tetris::TetrisScene>(); }, SceneID::Tetris);
                                }
                                else if (*choice == almondnamespace::menu::Choice::Pacman) {
                                    begin_scene([] { return std::make_unique<almondnamespace::pacman::PacmanScene>(); }, SceneID::Pacman);
                                }
                                else if (*choice == almondnamespace::menu::Choice::Sokoban) {
                                    begin_scene([] { return std::make_unique<almondnamespace::sokoban::SokobanScene>(); }, SceneID::Sokoban);
                                }
                                else if (*choice == almondnamespace::menu::Choice::Bejeweled) {
                                    begin_scene([] { return std::make_unique<almondnamespace::match3::Match3Scene>(); }, SceneID::Match3);
                                }
                                else if (*choice == almondnamespace::menu::Choice::Puzzle) {
                                    begin_scene([] { return std::make_unique<almondnamespace::sliding::SlidingScene>(); }, SceneID::Sliding);
                                }
                                else if (*choice == almondnamespace::menu::Choice::Minesweep) {
                                    begin_scene([] { return std::make_unique<almondnamespace::minesweeper::MinesweeperScene>(); }, SceneID::Minesweeper);
                                }
                                else if (*choice == almondnamespace::menu::Choice::Fourty) {
                                    begin_scene([] { return std::make_unique<almondnamespace::game2048::Game2048Scene>(); }, SceneID::Game2048);
                                }
                                else if (*choice == almondnamespace::menu::Choice::Sandsim) {
                                    begin_scene([] { return std::make_unique<almondnamespace::sandsim::SandSimScene>(); }, SceneID::Sandsim);
                                }
                                else if (*choice == almondnamespace::menu::Choice::Cellular) {
                                    begin_scene([] { return std::make_unique<almondnamespace::cellular::CellularScene>(); }, SceneID::Cellular);
                                }
                                else if (*choice == almondnamespace::menu::Choice::Settings) {
                                    std::cout << "[Menu] Settings selected.";
                                }
                                else if (*choice == almondnamespace::menu::Choice::Exit) {
                                    g_sceneID = SceneID::Exit;
                                    running = false;
                                }
                            }
                            break;
                        }
                        case SceneID::Snake:
                        case SceneID::Tetris:
                        case SceneID::Pacman:
                        case SceneID::Sokoban:
                        case SceneID::Match3:
                        case SceneID::Sliding:
                        case SceneID::Minesweeper:
                        case SceneID::Game2048:
                        case SceneID::Sandsim:
                        case SceneID::Cellular:
                            if (g_activeScene) {
                                ctxRunning = g_activeScene->frame(ctx, win);
                                if (!ctxRunning) {
                                    g_activeScene->unload();
                                    g_activeScene.reset();
                                    g_sceneID = SceneID::Menu;
                                    init_menu();
                                }
                            }
                            break;
                        case SceneID::Exit:
                            running = false;
                            break;
                        }

                        if (!ctxRunning) {
                            lastFrameTimes.erase(rawCtx);
                        }

                        return ctxRunning;
                    };

#if defined(ALMOND_SINGLE_PARENT)
                    if (!contexts.empty()) {
                        auto masterCtx = contexts.front();
                        if (masterCtx && !update_on_ctx(masterCtx)) { running = false; break; }
                        for (size_t i = 1; i < contexts.size(); ++i) {
                            if (!update_on_ctx(contexts[i])) running = false;
                        }
                    }
#else
                    bool anyAlive = false;
                    for (size_t i = 0; i < contexts.size(); ++i) {
                        auto& ctx = contexts[i];
                        if (!ctx) continue;
                        bool alive = update_on_ctx(ctx);
                        if (alive) {
                            anyAlive = true;
                        }
                        else if (i > 0) {
                            ctx->running = false;
                        }
                    }
                    if (!anyAlive) {
                        std::cout << "[Engine] All contexts for backend "
                            << int(type) << " closed.";
                    }
#endif
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }

            if (g_activeScene) {
                g_activeScene->unload();
                g_activeScene.reset();
            }
            menu.cleanup();
            mgr.StopAll();

            auto backendContexts = collect_backend_contexts();
            for (auto& [type, contexts] : backendContexts) {
                auto cleanup_backend = [&](std::shared_ptr<almondnamespace::core::Context> ctx) {
                    if (!ctx) return;
                    switch (type) {
#ifdef ALMOND_USING_OPENGL
                    case almondnamespace::core::ContextType::OpenGL:
                        almondnamespace::openglcontext::opengl_cleanup(ctx);
                        break;
#endif
#ifdef ALMOND_USING_SOFTWARE_RENDERER
                    case almondnamespace::core::ContextType::Software:
                        almondnamespace::anativecontext::softrenderer_cleanup(ctx);
                        break;
#endif
#ifdef ALMOND_USING_SDL
                    case almondnamespace::core::ContextType::SDL:
                        almondnamespace::sdlcontext::sdl_cleanup(ctx);
                        break;
#endif
#ifdef ALMOND_USING_SFML
                    case almondnamespace::core::ContextType::SFML:
                        almondnamespace::sfmlcontext::sfml_cleanup(ctx);
                        break;
#endif
#ifdef ALMOND_USING_RAYLIB
                    case almondnamespace::core::ContextType::RayLib:
                        almondnamespace::raylibcontext::raylib_cleanup(ctx);
                        break;
#endif
                    default: break;
                    }
                };

                for (auto& ctx : contexts) {
                    cleanup_backend(ctx);
                }
            }

            return 0;
        }

#if defined(_WIN32)
        int RunEngineMainLoopInternal(HINSTANCE hInstance, int nCmdShow)
        {
            UNREFERENCED_PARAMETER(nCmdShow);

            try {
                int32_t numCmdLineArgs = 0;
                LPWSTR* pCommandLineArgvArray =
                    CommandLineToArgvW(GetCommandLineW(), &numCmdLineArgs);
                if (!pCommandLineArgvArray) {
                    MessageBoxW(NULL, L"Command line parsing failed!",
                        L"Error", MB_ICONERROR | MB_OK);
                    return -1;
                }
                LocalFree(pCommandLineArgvArray);

                almondnamespace::core::MultiContextManager mgr;
                HINSTANCE hi = hInstance ? hInstance : GetModuleHandle(nullptr);
                bool multiOk = mgr.Initialize(hi,
                    /*RayLib*/   1,
                    /*SDL*/      1,
                    /*SFML*/     0,
                    /*OpenGL*/   1,
                    /*Software*/ 1,
                    ALMOND_SINGLE_PARENT == 1);

                if (!multiOk) {
                    MessageBoxW(NULL, L"Failed to initialize contexts!",
                        L"Error", MB_ICONERROR | MB_OK);
                    return -1;
                }

                almondnamespace::input::designate_polling_thread_to_current();

                mgr.StartRenderThreads();
                mgr.ArrangeDockedWindowsGrid();

                auto pump = []() -> bool {
                    MSG msg{};
                    bool keepRunning = true;
                    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                        if (msg.message == WM_QUIT) {
                            keepRunning = false;
                        }
                        else {
                            TranslateMessage(&msg);
                            DispatchMessage(&msg);
                        }
                    }

                    if (!keepRunning) {
                        return false;
                    }

                    almondnamespace::input::poll_input();
                    return true;
                };

                return RunEngineMainLoopCommon(mgr, pump);
            }
            catch (const std::exception& ex) {
                MessageBoxA(nullptr, ex.what(), "Error", MB_ICONERROR | MB_OK);
                return -1;
            }
        }
#elif defined(__linux__)
        int RunEngineMainLoopLinux()
        {
            try {
                almondnamespace::core::MultiContextManager mgr;
                bool multiOk = mgr.Initialize(nullptr,
                    /*RayLib*/   1,
                    /*SDL*/      1,
                    /*SFML*/     0,
                    /*OpenGL*/   1,
                    /*Software*/ 1,
                    false);

                if (!multiOk) {
                    std::cerr << "[Engine] Failed to initialize contexts!\n";
                    return -1;
                }

                almondnamespace::input::designate_polling_thread_to_current();

                mgr.StartRenderThreads();
                mgr.ArrangeDockedWindowsGrid();

                auto pump = []() -> bool {
                    return almondnamespace::platform::pump_events();
                };

                return RunEngineMainLoopCommon(mgr, pump);
            }
            catch (const std::exception& ex) {
                std::cerr << "[Engine] " << ex.what() << '\n';
                return -1;
            }
        }
#endif
    } // namespace

    void RunEngine()
    {
#if defined(_WIN32)
    #if defined(RUN_CODE_INSPECTOR)
        using namespace almondnamespace;
        codeinspector::LineStats totalStats{};
        auto results = codeinspector::inspect_directory(
            "C:/Users/iammi/source/repos/NewEngine/AlmondShell/", totalStats);
        for (const auto& res : results) {
            std::cout << "File: \"" << res.filePath.string() << "\"\n";
            for (const auto& issue : res.issues)
                std::cout << "  - " << issue << '\n';
            std::cout << '\n';
        }
        std::cout << "=== Combined Totals Across All Files ===\n";
        for (const auto& line : totalStats.format_summary())
            std::cout << line << '\n';
    #endif
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        const int result = RunEngineMainLoopInternal(instance, SW_SHOWNORMAL);
        if (result != 0) {
            std::cerr << "[Engine] RunEngine terminated with code " << result << "\n";
        }
#elif defined(__linux__)
        const int result = RunEngineMainLoopLinux();
        if (result != 0) {
            std::cerr << "[Engine] RunEngine terminated with code " << result << "\n";
        }
#else
        std::cerr << "[Engine] RunEngine is not implemented for this platform yet.\n";
#endif
    }

        // ─── 2) Start the engine in WinMain or main() ──────────────────────────────
    void StartEngine() {

		// Print engine version
		std::cout << "AlmondShell Engine v" << almondnamespace::GetEngineVersion() << '\n';

       // bool multicontext = false;// cli::multi_context;
        //int choice;

    //    while (true) {
    //        std::cout << "1. Single\n"
    //            << "2. Multi\n"
    //            << "> ";

    //        std::cin >> choice;

    //        if (std::cin.fail()) {
    //            // Handle non-integer input
    //            std::cin.clear();
    //            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    //            std::cout << "Invalid input. Please enter a number.\n";
    //            continue;
    //        }

    //        if (choice == 1) {
    //            std::cout << "Selected: Single\n";
				//multicontext = false; // Single context mode
    //            break;
    //        }
    //        else if (choice == 2) {
    //            std::cout << "Selected: Multi\n";
				//multicontext = true; // Multi context mode
    //            break;
    //        }
    //        else {
    //            std::cout << "Invalid choice. Please enter 1 or 2.\n";
    //        }
    //    }

    //    if (multicontext == true)
    //    {

        //}
      //  else
        //{
            // Main engine loop - single context
            RunEngine();
        //}
    }


    struct TextureUploadTask
    {
        //GLuint texture;
        int w, h;
        const void* pixels;
    };

    struct TextureUploadQueue
    {
        std::queue<TextureUploadTask> tasks;
        std::mutex mtx;
    public:
        void push(TextureUploadTask&& task) {
            std::lock_guard lock(mtx);
            tasks.push(std::move(task));
        }
        std::optional<TextureUploadTask> try_pop() {
            std::lock_guard lock(mtx);
            if (tasks.empty()) return {};
            auto task = tasks.front();
            tasks.pop();
            return task;
        }
    };

    inline std::vector<std::unique_ptr<TextureUploadQueue>> uploadQueues;

#if defined(_WIN32) && defined(ALMOND_USING_WINMAIN)


    void PromoteToTopLevel(HWND hwnd) {
        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);

        style &= ~WS_CHILD;                // Remove child style
        style |= WS_OVERLAPPEDWINDOW;     // Add top-level window styles

        SetWindowLongPtr(hwnd, GWL_STYLE, style);
        SetParent(hwnd, nullptr);

        SetWindowPos(hwnd, HWND_TOP, 100, 100, 400, 300,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        ShowWindow(hwnd, SW_SHOW);
    }


//}


    // Function to show the console in debug mode
// ShowConsole() in your core namespace
    void ShowConsole() {
#ifdef _DEBUG       // “if I’m in a Debug build”
        AllocConsole();
        FILE* fp = nullptr;
        freopen_s(&fp, "CONIN$", "r", stdin);
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
#else
        FreeConsole(); // if you somehow still have one
#endif
    }

    //HWND create_window(HINSTANCE hInstance, int x, int y, int w, int h, const wchar_t* cname, const wchar_t* title, HWND parent) {
    //    DWORD style = (parent) ? (WS_CHILD | WS_VISIBLE | WS_BORDER | WS_CLIPSIBLINGS | WS_CLIPCHILDREN) : (WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    //    HWND hwnd = CreateWindowEx(0, cname, title, style, x, y, w, h, parent, nullptr, hInstance, nullptr);
    //    // if (!hwnd) {
    //    //     std::wcerr << L"Failed to create window: " << title << L"\n";
    //    //     return nullptr;
    //    // }
    //    ShowWindow(hwnd, SW_SHOW);
    //    UpdateWindow(hwnd);
    //    return hwnd;
    //}
//}

// Tell the linker which subsystem to use:
#ifdef NDEBUG     // in Release
  #pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#else              // in Debug
  #pragma comment(linker, "/SUBSYSTEM:CONSOLE")
#endif

    // Define global variables here
WCHAR szTitle[MAX_LOADSTRING] = L"Automatic WinMain Example";
WCHAR szChildTitle[MAX_LOADSTRING] = L"Automatic WinMain With Child Example";
WCHAR szWindowClass[MAX_LOADSTRING] = L"SampleWindowClass";

almondnamespace::contextwindow::WindowData windowContext{};

#endif // defined(_WIN32) && defined(ALMOND_USING_WINMAIN)

} // namespace almondnamespace::core

/////////////////////////////////////////////////////////

// this basically just leaves ninja.zip when commented out, but will be configured better in the future
#define LEAVE_NO_FILES_ALWAYS_REDOWNLOAD

//using namespace almondnamespace::core;

// configuration overrides
namespace urls {
    const std::string github_base = "https://github.com/"; // base github url
    const std::string github_raw_base = "https://raw.githubusercontent.com/"; // raw base github url, for source downloads

    const std::string owner = "Autodidac/"; // github project developer username for url
    const std::string repo = "Cpp_Ultimate_Project_Updater"; // whatever your github project name is
    const std::string branch = "main/"; // incase you need a different branch than githubs default branch main

    // It's now using this internal file to fetch update versions internally without version.txt file that can be modified
    const std::string version_url = github_raw_base + owner + repo + "/" + branch + "/include/config.hpp";
    //const std::string source_url = github_base + owner + repo + "/archive/refs/heads/main.zip";
    const std::string binary_url = github_base + owner + repo + "/releases/latest/download/updater.exe";
}



#if defined(_WIN32)
//// Now ALWAYS define WinMain so the linker will find it
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

#ifdef _DEBUG
    almondnamespace::core::ShowConsole();
#endif


    try {
        // Command line processing
        int argc = __argc;
        char** argv = __argv;

        // Convert command line to wide string array (LPWSTR)
        int32_t numCmdLineArgs = 0;
        LPWSTR* pCommandLineArgvArray = CommandLineToArgvW(GetCommandLineW(), &numCmdLineArgs);

        if (pCommandLineArgvArray == nullptr) {
            MessageBoxW(NULL, L"Command line parsing failed!", L"Error", MB_ICONERROR | MB_OK);
            return -1;
        }

        // Free the argument array after use
        LocalFree(pCommandLineArgvArray);

        const auto cli_result = almondnamespace::core::cli::parse(argc, argv);

        const almondnamespace::updater::UpdateChannel channel{
            .version_url = urls::version_url,
            .binary_url = urls::binary_url,
        };

        if (cli_result.update_requested) {
            const auto update_result = almondnamespace::updater::run_update_command(channel, cli_result.force_update);
            return update_result.exit_code;
        }

        LPCWSTR window_name = L"Almond Example Window";


    }
    catch (const std::exception& ex) {
        MessageBoxA(nullptr, ex.what(), "Error", MB_ICONERROR | MB_OK);
        return -1;
    }

    return almondnamespace::core::RunEngineMainLoopInternal(hInstance, SW_SHOWNORMAL);
}
#endif // defined(_WIN32)


// -----------------------------------------------------------------------------
// Cross-platform Automatic Entry Point
// -----------------------------------------------------------------------------
int main(int argc, char* argv[]) 
{

#if defined(_WIN32) && defined(ALMOND_USING_WINMAIN)
    LPWSTR pCommandLine = GetCommandLineW();
    return wWinMain(GetModuleHandle(NULL), NULL, pCommandLine, SW_SHOWNORMAL);


#else
    const auto cli_result = almondnamespace::core::cli::parse(argc, argv);

    const almondnamespace::updater::UpdateChannel channel
    {
        .version_url = urls::version_url,
        .binary_url = urls::binary_url,
    };

    if (cli_result.update_requested) 
    {
        const auto update_result = almondnamespace::updater::run_update_command(channel, cli_result.force_update);
        return update_result.exit_code;
    }


    almondnamespace::core::StartEngine(); // Replace with actual engine logic
    return 0;

#endif
}
