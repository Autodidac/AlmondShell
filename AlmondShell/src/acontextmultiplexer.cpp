//acontextmultiplexer.cpp
#include "pch.h"

#include "acontextmultiplexer.hpp"

#include "aopenglcontext.hpp"
#include "asdlcontext.hpp"
#include "asfmlcontext.hpp"
#include "araylibcontext.hpp"
#include "asoftrenderer_context.hpp"

#include <stdexcept>
#include <algorithm>
#include <glad/glad.h>
#include <memory>
#include <shared_mutex>

// -----------------------------------------------------------------
// Helper definitions that must be visible to the linker
// -----------------------------------------------------------------

// -----------------------------------------------------------------
// Dockable child handling
// -----------------------------------------------------------------
struct SubCtx { HWND originalParent; };

LRESULT CALLBACK DockableProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
    UINT_PTR, DWORD_PTR dw)
{
#if ALMOND_SINGLE_PARENT
    auto* ctx = reinterpret_cast<SubCtx*>(dw);
#endif

    switch (msg) {
    case WM_CLOSE:
#if ALMOND_SINGLE_PARENT
        if (ctx && ctx->originalParent) {
            if (hwnd == ctx->originalParent) {
                PostQuitMessage(0); // main parent closed -> shutdown engine
            }
            else {
                DestroyWindow(hwnd); // child closes itself
            }
        }
        else {
            DestroyWindow(hwnd); // no parent -> just close itself
        }
#else
        DestroyWindow(hwnd); // always independent
#endif
        return 0;

    case WM_LBUTTONDOWN:
    case WM_MOUSEMOVE:
    case WM_LBUTTONUP:
        return almondnamespace::core::MultiContextManager::ChildProc(hwnd, msg, wp, lp);
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

void MakeDockable(HWND hwnd, HWND parent) {
#if ALMOND_SINGLE_PARENT
    auto* ctx = new SubCtx{ parent };
    SetWindowSubclass(hwnd, DockableProc, 1, reinterpret_cast<DWORD_PTR>(ctx));
#endif
}

namespace almondnamespace::core
{

    std::unordered_map<HWND, std::thread> gThreads{};
    DragState gDragState{};

    WindowData* MultiContextManager::findWindowByHWND(HWND hwnd) {
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [hwnd](const std::unique_ptr<WindowData>& w) {
                return w && w->hwnd == hwnd;
            });
        return (it != windows.end()) ? it->get() : nullptr;
    }

    const WindowData* MultiContextManager::findWindowByHWND(HWND hwnd) const {
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [hwnd](const std::unique_ptr<WindowData>& w) {
                return w && w->hwnd == hwnd;
            });
        return (it != windows.end()) ? it->get() : nullptr;
    }

    WindowData* MultiContextManager::findWindowByContext(const std::shared_ptr<core::Context>& ctx) {
        if (!ctx) return nullptr;
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [&](const std::unique_ptr<WindowData>& w) {
                return w && w->context && w->context.get() == ctx.get();
            });
        return (it != windows.end()) ? it->get() : nullptr;
    }

    const WindowData* MultiContextManager::findWindowByContext(const std::shared_ptr<core::Context>& ctx) const {
        if (!ctx) return nullptr;
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [&](const std::unique_ptr<WindowData>& w) {
                return w && w->context && w->context.get() == ctx.get();
            });
        return (it != windows.end()) ? it->get() : nullptr;
    }

    // ======================================================
    // Inline Implementations
    // ======================================================

    // ---------------- MultiContextManager (static) ----------------
    //inline std::shared_ptr<core::Context> MultiContextManager::currentContext = nullptr;

    inline void almondnamespace::core::MultiContextManager::SetCurrent(std::shared_ptr<core::Context> ctx) {
        currentContext = std::move(ctx);
    }

    std::shared_ptr<almondnamespace::core::Context> almondnamespace::core::MultiContextManager::GetCurrent() {
        return currentContext;
    }

    // ---------------- MultiContextManager (public helpers) ----------------
    inline bool MultiContextManager::IsRunning() const noexcept {
        return running.load(std::memory_order_acquire);
    }

    inline void MultiContextManager::StopRunning() noexcept {
        running.store(false, std::memory_order_release);
    }

    inline void MultiContextManager::EnqueueRenderCommand(HWND hwnd, RenderCommand cmd) {
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [hwnd](const std::unique_ptr<WindowData>& w) { return w->hwnd == hwnd; });
        if (it != windows.end()) {
            (*it)->EnqueueCommand(std::move(cmd));
        }
    }

    // -----------------------------------------------------------------
    // Implementation
    // -----------------------------------------------------------------
    void MultiContextManager::ShowConsole()
    {
        if (AllocConsole()) {
            FILE* f;
            freopen_s(&f, "CONOUT$", "w", stdout);
            freopen_s(&f, "CONIN$", "r", stdin);
            freopen_s(&f, "CONOUT$", "w", stderr);
            std::ios::sync_with_stdio(true);
        }
    }

    ATOM MultiContextManager::RegisterParentClass(HINSTANCE hInst, LPCWSTR name)
    {
        WNDCLASS wc{};
        wc.lpfnWndProc = ParentProc;
        wc.hInstance = hInst;
        wc.lpszClassName = name;
        wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        return RegisterClass(&wc);
    }

    ATOM MultiContextManager::RegisterChildClass(HINSTANCE hInst, LPCWSTR name)
    {
        WNDCLASS wc{};
        wc.lpfnWndProc = ChildProc;
        wc.hInstance = hInst;
        wc.lpszClassName = name;
        wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        return RegisterClass(&wc);
    }

    void MultiContextManager::SetupPixelFormat(HDC hdc)
    {
        PIXELFORMATDESCRIPTOR pfd{};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.iLayerType = PFD_MAIN_PLANE;

        int pf = ChoosePixelFormat(hdc, &pfd);
        if (pf == 0 || !SetPixelFormat(hdc, pf, &pfd))
            throw std::runtime_error("Failed to set pixel format");
    }

    HGLRC MultiContextManager::CreateSharedGLContext(HDC hdc)
    {
        SetupPixelFormat(hdc);
        HGLRC ctx = wglCreateContext(hdc);
        if (!ctx) throw std::runtime_error("Failed to create OpenGL context");
        if (sharedContext && !wglShareLists(sharedContext, ctx)) {
            wglDeleteContext(ctx);
            throw std::runtime_error("Failed to share GL context");
        }
        return ctx;
    }

    //bool almondnamespace::core::MultiContextManager::ProcessBackend(ContextType type) {
    //    auto it = g_backends.find(type);
    //    if (it == g_backends.end()) return false;

    //    auto& state = it->second;
    //    bool anyRunning = false;

    //    if (state.master && state.master->process) {
    //        anyRunning |= state.master->process_safe(*state.master);
    //    }

    //    for (auto& dup : state.duplicates) {
    //        if (dup && dup->process) {
    //            anyRunning |= dup->process_safe(*dup);
    //        }
    //    }

    //    return anyRunning;
    //}

    inline int MultiContextManager::get_title_bar_thickness(const HWND window_handle)
    {
        RECT window_rectangle{}, client_rectangle{};
        GetWindowRect(window_handle, &window_rectangle);
        GetClientRect(window_handle, &client_rectangle);

        int totalWidth = window_rectangle.right - window_rectangle.left;
        int totalHeight = window_rectangle.bottom - window_rectangle.top;
        int clientWidth = client_rectangle.right - client_rectangle.left;
        int clientHeight = client_rectangle.bottom - client_rectangle.top;

        int nonClientHeight = totalHeight - clientHeight; // title bar + borders (top+bottom)
        int nonClientWidth = totalWidth - clientWidth;   // vertical borders (left+right)

        int borderThickness = nonClientWidth / 2;          // assume symmetrical
        int titleBarHeight = nonClientHeight - borderThickness * 2;

#if defined(DEBUG_WINDOW_VERBOSE)
        std::cout << "Window size:  " << totalWidth << "x" << totalHeight << " px\n";
        std::cout << "Client size:  " << clientWidth << "x" << clientHeight << " px\n";
        std::cout << "Non-client H: " << nonClientHeight
            << " (title bar + top/bottom borders)\n";
        std::cout << "Non-client W: " << nonClientWidth
            << " (left+right borders total)\n";
        std::cout << "Border thickness: " << borderThickness << " px each side\n";
        std::cout << "Title bar height: " << titleBarHeight << " px\n";
#endif
        return titleBarHeight;
    }

    // ----------  MultiContext Manager Initialize  -------------------------------------------------------
    bool MultiContextManager::Initialize(HINSTANCE hInst,
        int RayLibWinCount, int SDLWinCount, int SFMLWinCount,
        int OpenGLWinCount, int SoftwareWinCount, bool parented)
    {
        const int totalRequested =
            RayLibWinCount + SDLWinCount + SFMLWinCount + OpenGLWinCount + SoftwareWinCount;
        if (totalRequested <= 0) return false;

        s_activeInstance = this;

        RegisterParentClass(hInst, L"AlmondParent");
        RegisterChildClass(hInst, L"AlmondChild");

        InitializeAllContexts();

        // ---------------- Parent (dock container) ----------------
        if (parented) {
            int cols = 1, rows = 1;
            while (cols * rows < totalRequested) (cols <= rows ? ++cols : ++rows);

            int cellW = (totalRequested == 1) ? cli::window_width : 400;
            int cellH = (totalRequested == 1) ? cli::window_height : 300;

            int clientW = cols * cellW;
            int clientH = rows * cellH;

            RECT want = { 0, 0, clientW, clientH };
            DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
            AdjustWindowRect(&want, style, FALSE);

            parent = CreateWindowEx(
                0, L"AlmondParent", L"Almond Docking",
                style,
                CW_USEDEFAULT, CW_USEDEFAULT,
                want.right - want.left, want.bottom - want.top,
                nullptr, nullptr, hInst, this);

            if (!parent) return false;

            DragAcceptFiles(parent, TRUE);
        }
        else {
            parent = nullptr;
        }

#ifdef ALMOND_USING_OPENGL
        // ---------------- Shared dummy GL context (for wglShareLists) ----------------
        {
            HWND dummy = CreateWindowEx(WS_EX_TOOLWINDOW, L"AlmondChild", L"Dummy",
                WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, hInst, nullptr);
            if (!dummy) return false;

            HDC dummyDC = GetDC(dummy);
            SetupPixelFormat(dummyDC);
            sharedContext = wglCreateContext(dummyDC);
            if (!sharedContext) {
                ReleaseDC(dummy, dummyDC);
                DestroyWindow(dummy);
                throw std::runtime_error("Failed to create shared OpenGL context");
            }
            wglMakeCurrent(dummyDC, sharedContext);

            static bool gladInitialized = false;
            if (!gladInitialized) {
                gladInitialized = gladLoadGL();
                std::cerr << "[Init] GLAD loaded on dummy context\n";
            }
            wglMakeCurrent(nullptr, nullptr);
            ReleaseDC(dummy, dummyDC);
            DestroyWindow(dummy);
        }
#endif

        // ---------------- Helper: create N windows for a backend ----------------
        auto make_backend_windows = [&](ContextType type, int count) {
            if (count <= 0) return;

            std::vector<HWND> created;
            created.reserve(count);

            for (int i = 0; i < count; ++i) {
                HWND hwnd = CreateWindowEx(
                    0, L"AlmondChild", L"",
                    (parent ? WS_CHILD | WS_VISIBLE : WS_OVERLAPPEDWINDOW | WS_VISIBLE),
                    0, 0, 10, 10,
                    parent, nullptr, hInst, nullptr);

                if (!hwnd) continue;

                HDC hdc = GetDC(hwnd);
                HGLRC glrc = nullptr;

#ifdef ALMOND_USING_OPENGL
                if (type == ContextType::OpenGL) {
                    glrc = CreateSharedGLContext(hdc);
                }
#endif

                auto winPtr = std::make_unique<WindowData>(hwnd, hdc, glrc, true, type);
                if (parent) MakeDockable(hwnd, parent);
                {
                    std::scoped_lock lock(windowsMutex);
                    windows.emplace_back(std::move(winPtr));
                }
                created.push_back(hwnd);
            }

            std::vector<std::shared_ptr<Context>> ctxs;
            {
                std::unique_lock lock(g_backendsMutex);
                auto it = g_backends.find(type);
                if (it == g_backends.end() || !it->second.master) {
                    std::cerr << "[Init] Missing prototype context for backend type "
                        << static_cast<int>(type) << "\n";
                    return;
                }

                BackendState& state = it->second;
                ctxs.reserve(static_cast<size_t>(count));
                ctxs.push_back(state.master);

                auto ensure_duplicate = [&](size_t index) -> std::shared_ptr<Context> {
                    if (index < state.duplicates.size()) {
                        auto& candidate = state.duplicates[index];
                        if (candidate && candidate->initialize) {
                            return candidate;
                        }
                        candidate = CloneContext(*state.master);
                        return candidate;
                    }
                    auto dup = CloneContext(*state.master);
                    state.duplicates.push_back(dup);
                    return dup;
                };

                while (ctxs.size() < static_cast<size_t>(count)) {
                    ctxs.push_back(ensure_duplicate(ctxs.size() - 1));
                }
            }

            const size_t n = std::min(created.size(), ctxs.size());
            for (size_t i = 0; i < n; ++i) {
                HWND hwnd = created[i];
                auto* w = findWindowByHWND(hwnd);
                auto& ctx = ctxs[i];
                if (!ctx) continue;

                ctx->type = type;
                ctx->hwnd = hwnd;
                RECT rc{};
                if (parent) GetClientRect(parent, &rc);
                else GetClientRect(hwnd, &rc);
                const int width = std::max<LONG>(1, rc.right - rc.left);
                const int height = std::max<LONG>(1, rc.bottom - rc.top);
                ctx->width = width;
                ctx->height = height;

                if (w) {
                    ctx->hdc = w->hdc;
                    ctx->hglrc = w->glContext;
                    ctx->windowData = w;
                    w->context = ctx;
                    w->width = width;
                    w->height = height;
                    if (!ctx->onResize && w->onResize)
                        ctx->onResize = w->onResize;
                }

                // ---------------- Immediate backend initialization ----------------
                switch (type) {
#ifdef ALMOND_USING_OPENGL
                case ContextType::OpenGL:
                    if (ctx->hdc && ctx->hglrc) {
                        if (!wglMakeCurrent(ctx->hdc, ctx->hglrc)) {
                            std::cerr << "[Init] wglMakeCurrent failed for hwnd=" << hwnd << "\n";
                        }
                        else {
                            std::cerr << "[Init] Running OpenGL init for hwnd=" << hwnd << "\n";
                            almondnamespace::openglcontext::opengl_initialize(
                                ctx, hwnd, ctx->width, ctx->height, w->onResize);
                            wglMakeCurrent(nullptr, nullptr);
                        }
                    }
                    break;
#endif
#ifdef ALMOND_USING_SOFTWARE_RENDERER
                case ContextType::Software:
                    std::cerr << "[Init] Initializing Software renderer for hwnd=" << hwnd << "\n";
                    almondnamespace::anativecontext::softrenderer_initialize(
                        ctx, hwnd, ctx->width, ctx->height, w->onResize);
                    break;
#endif
#ifdef ALMOND_USING_SDL
                case ContextType::SDL:
                    std::cerr << "[Init] Initializing SDL context for hwnd=" << hwnd << "\n";
                    almondnamespace::sdlcontext::sdl_initialize(
                        ctx, hwnd, ctx->width, ctx->height, w ? w->onResize : nullptr);
                    break;
#endif
#ifdef ALMOND_USING_SFML
                case ContextType::SFML:
                    std::cerr << "[Init] Initializing SFML context for hwnd=" << hwnd << "\n";
                    almondnamespace::sfmlcontext::sfml_initialize(
                        ctx, hwnd, ctx->width, ctx->height, w->onResize);
                    break;
#endif
#ifdef ALMOND_USING_RAYLIB
                case ContextType::RayLib:
                    std::cerr << "[Init] Initializing RayLib context for hwnd=" << hwnd << "\n";
                    almondnamespace::raylibcontext::raylib_initialize(
                        ctx, hwnd, ctx->width, ctx->height, w ? w->onResize : nullptr);
                    break;
#endif
                default:
                    break;
                }
            }
            };

#ifdef ALMOND_USING_RAYLIB
        make_backend_windows(ContextType::RayLib, RayLibWinCount);
#endif
#ifdef ALMOND_USING_SDL
        make_backend_windows(ContextType::SDL, SDLWinCount);
#endif
#ifdef ALMOND_USING_SFML
        make_backend_windows(ContextType::SFML, SFMLWinCount);
#endif
#ifdef ALMOND_USING_OPENGL
        make_backend_windows(ContextType::OpenGL, OpenGLWinCount);
#endif
#ifdef ALMOND_USING_SOFTWARE_RENDERER
        make_backend_windows(ContextType::Software, SoftwareWinCount);
#endif

        ArrangeDockedWindowsGrid();
        {
            std::shared_lock lock(g_backendsMutex);
            return !g_backends.empty();
        }
    }

    //void almondnamespace::core::MultiContextManager::AddWindow(HWND hwnd, HDC hdc, HGLRC glContext,
    //    bool usesSharedContext, ResizeCallback onResize) {
    //    windows.emplace_back(hwnd, hdc, glContext, usesSharedContext);
    //    windows.back().onResize = std::move(onResize);
    //    if (!hdc) MakeDockable(hwnd, parent);
    //}

    // ======================================================
    // MultiContextManager : Implementations
    // ======================================================
    void MultiContextManager::AddWindow(
        HWND hwnd,
        HWND parent,
        HDC hdc,
        HGLRC glContext,
        bool usesSharedContext,
        ResizeCallback onResize,
        ContextType type)
    {
        if (!hwnd) return;

        s_activeInstance = this;

        if (!hdc) hdc = GetDC(hwnd);

        // Only create GL context if this window is OpenGL
        if (type == ContextType::OpenGL) {
#ifdef ALMOND_USING_OPENGL
            if (!glContext) {
                SetupPixelFormat(hdc);
                glContext = CreateSharedGLContext(hdc);

                static bool gladInitialized = false;
                if (!gladInitialized) {
                    wglMakeCurrent(hdc, glContext);
                    gladInitialized = gladLoadGL();
                    wglMakeCurrent(nullptr, nullptr);
                }
            }
#endif
        }

        // Make window dockable if a parent is provided
        if (parent) MakeDockable(hwnd, parent);

        bool needInit = false;
        {
            std::shared_lock lock(g_backendsMutex);
            needInit = g_backends.empty();
        }
        if (needInit) {
            InitializeAllContexts();
        }

        std::shared_ptr<core::Context> ctx;
        {
            std::unique_lock lock(g_backendsMutex);
            auto& state = g_backends[type];
            if (!state.master) {
                ctx = std::make_shared<core::Context>();
                ctx->type = type;
                state.master = ctx;
            }
            else if (!state.master->windowData) {
                ctx = state.master;
            }
            else {
                auto it = std::find_if(state.duplicates.begin(), state.duplicates.end(),
                    [](const std::shared_ptr<core::Context>& dup) {
                        return dup && !dup->windowData;
                    });
                if (it != state.duplicates.end()) {
                    ctx = *it;
                }
                else {
                    auto dup = CloneContext(*state.master);
                    state.duplicates.push_back(dup);
                    ctx = dup;
                }
            }
        }

        if (ctx)
            ctx->type = type;

        ctx->hwnd = hwnd;
        ctx->hdc = hdc;
        ctx->hglrc = glContext;
        ctx->type = type;

        // Prepare WindowData (unique_ptr)
        auto winPtr = std::make_unique<WindowData>(hwnd, hdc, glContext, usesSharedContext, type);
        winPtr->onResize = std::move(onResize);
        winPtr->context = ctx;  // <-- hook it up
        ctx->windowData = winPtr.get(); // set the back-pointer immediately

        RECT rc{};
        GetClientRect(hwnd, &rc);
        const int width = std::max<LONG>(1, rc.right - rc.left);
        const int height = std::max<LONG>(1, rc.bottom - rc.top);
        ctx->width = width;
        ctx->height = height;
        winPtr->width = width;
        winPtr->height = height;
        if (!ctx->onResize && winPtr->onResize) ctx->onResize = winPtr->onResize;
        if (!winPtr->onResize && ctx->onResize) winPtr->onResize = ctx->onResize;

        // Keep raw pointer for thread use
        WindowData* rawWinPtr = winPtr.get();

        {
            std::scoped_lock lock(windowsMutex);
            windows.emplace_back(std::move(winPtr));
        }

        // Launch render thread if needed
        if (!gThreads.contains(hwnd) && rawWinPtr) {
            gThreads[hwnd] = std::thread([this, rawWinPtr]() {
                RenderLoop(*rawWinPtr);
                });
        }

        ArrangeDockedWindowsGrid();
    }


    void MultiContextManager::HandleResize(HWND hwnd, int width, int height)
    {
        if (!hwnd) return;

        const int clampedWidth = std::max(1, width);
        const int clampedHeight = std::max(1, height);

        std::function<void(int, int)> resizeCallback;
        WindowData* window = nullptr;

        {
            std::scoped_lock lock(windowsMutex);
            auto it = std::find_if(windows.begin(), windows.end(),
                [hwnd](const std::unique_ptr<WindowData>& w) { return w && w->hwnd == hwnd; });
            if (it == windows.end()) {
                return;
            }

            window = it->get();
            window->width = clampedWidth;
            window->height = clampedHeight;

            if (window->context) {
                window->context->width = clampedWidth;
                window->context->height = clampedHeight;
                if (window->context->onResize) {
                    resizeCallback = window->context->onResize;
                }
            }

            if (!resizeCallback && window->onResize) {
                resizeCallback = window->onResize;
            }
        }

        if (resizeCallback && window) {
            window->commandQueue.enqueue([
                cb = std::move(resizeCallback),
                clampedWidth,
                clampedHeight
            ]() mutable {
                if (cb) cb(clampedWidth, clampedHeight);
            });
        }
    }


    void MultiContextManager::StartRenderThreads() {
        std::scoped_lock lock(windowsMutex); // lock while grabbing pointers
        for (const auto& w : windows) {
            if (!gThreads.contains(w->hwnd)) {
#if ALMOND_SINGLE_PARENT
                // Only spawn threads for windows that exist and are valid
                gThreads[w->hwnd] = std::thread([this, hwnd = w->hwnd]() {
                    auto it = std::find_if(windows.begin(), windows.end(),
                        [hwnd](const std::unique_ptr<WindowData>& w) { return w->hwnd == hwnd; });
                    if (it != windows.end()) RenderLoop(**it);
                    });
#else
                // Fully independent: spawn thread for every window
                gThreads[w->hwnd] = std::thread([this, hwnd = w->hwnd]() {
                    auto it = std::find_if(windows.begin(), windows.end(),
                        [hwnd](const std::unique_ptr<WindowData>& w) { return w->hwnd == hwnd; });
                    if (it != windows.end()) RenderLoop(**it);
                    });
#endif
            }
        }
    }

    void MultiContextManager::RemoveWindow(HWND hwnd)
    {
        std::unique_ptr<WindowData> removedWindow;

        {
            std::scoped_lock lock(windowsMutex);

            // Find the window
            auto it = std::find_if(
                windows.begin(), windows.end(),
                [hwnd](const std::unique_ptr<WindowData>& w) {
                    return w && w->hwnd == hwnd;
                });

            if (it == windows.end())
                return;

            // Mark the window as no longer running
            (*it)->running = false;

            if ((*it)->context) {
                auto ctx = (*it)->context;
                if (ctx->windowData == it->get()) {
                    ctx->windowData = nullptr;
                }
                if (ctx->hwnd == hwnd) {
                    ctx->hwnd = nullptr;
                    ctx->hdc = nullptr;
                    ctx->hglrc = nullptr;
                }
            }

            // Take ownership of the unique_ptr out of the vector
            removedWindow = std::move(*it);
            windows.erase(it);
        }

        // Ensure the render thread is stopped before destroying the context
        if (gThreads.contains(hwnd)) {
            if (gThreads[hwnd].joinable()) {
                gThreads[hwnd].join();
            }
            gThreads.erase(hwnd);
        }

        // Clean up OpenGL / DC (safe now, since thread is joined)
        if (removedWindow) {
            if (removedWindow->glContext) {
                wglMakeCurrent(nullptr, nullptr);
                wglDeleteContext(removedWindow->glContext);
            }
            if (removedWindow->hdc && removedWindow->hwnd) {
                ReleaseDC(removedWindow->hwnd, removedWindow->hdc);
            }
            // unique_ptr destructor cleans up WindowData
        }
    }

    // ---------------- Docking Layout ----------------

    // Original version (simple equal grid):
    // void almondnamespace::core::MultiContextManager::ArrangeDockedWindowsGrid() {
    //     if (!parent || windows.empty()) return;
    //     RECT rc; GetClientRect(parent, &rc);
    //     int cols = 1, rows = 1;
    //     while (cols * rows < (int)windows.size())
    //         (cols <= rows ? ++cols : ++rows);
    //     int cw = rc.right / cols, ch = rc.bottom / rows;
    //     for (size_t i = 0; i < windows.size(); ++i) {
    //         int c = int(i) % cols, r = int(i) / cols;
    //         SetWindowPos(windows[i]->hwnd, nullptr, c * cw, r * ch, cw, ch,
    //             SWP_NOZORDER | SWP_NOACTIVATE);
    //         if (windows[i]->onResize) windows[i]->onResize(cw, ch);
    //     }
    // }

    // Alternate version (resize parent to fit grid):
    // void almondnamespace::core::MultiContextManager::ArrangeDockedWindowsGrid() {
    //     if (!parent || windows.empty()) return;
    //
    //     constexpr int minCellWidth = 400;
    //     constexpr int minCellHeight = 300;
    //
    //     int cols = 1, rows = 1;
    //     while (cols * rows < (int)windows.size())one error is vector subscript out of range
    //         (cols <= rows ? ++cols : ++rows);
    //
    //     int totalWidth = cols * minCellWidth;
    //     int totalHeight = rows * minCellHeight;
    //
    //     SetWindowPos(parent, nullptr, 0, 0, totalWidth, totalHeight,
    //         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    //
    //     RECT rc; GetClientRect(parent, &rc);
    //     int cw = rc.right / cols;
    //     int ch = rc.bottom / rows;
    //
    //     for (size_t i = 0; i < windows.size(); ++i) {
    //         int c = int(i) % cols;
    //         int r = int(i) / cols;
    //         SetWindowPos(windows[i]->hwnd, nullptr, c * cw, r * ch, cw, ch,
    //             SWP_NOZORDER | SWP_NOACTIVATE);
    //         if (windows[i]->onResize) windows[i]->onResize(cw, ch);
    //     }
    // }

    // Final active version (your current one):
    void MultiContextManager::ArrangeDockedWindowsGrid() {
        if (!parent || windows.empty()) return;

        // Compute grid
        int total = static_cast<int>(windows.size());
        int cols = 1, rows = 1;
        while (cols * rows < total) (cols <= rows ? ++cols : ++rows);

        // Parent client area
        RECT rcClient{};
        GetClientRect(parent, &rcClient);
        int clientW = rcClient.right - rcClient.left;
        int clientH = rcClient.bottom - rcClient.top;

        int cw = std::max(1, clientW / cols);
        int ch = std::max(1, clientH / rows);

        // Place each child in its cell
        for (size_t i = 0; i < windows.size(); ++i) {
            int c = static_cast<int>(i) % cols;
            int r = static_cast<int>(i) / cols;

            WindowData& win = *windows[i];
            SetWindowPos(win.hwnd, nullptr, c * cw, r * ch, cw, ch,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

            HandleResize(win.hwnd, cw, ch);
        }
    }

    void MultiContextManager::StopAll() {
        running = false;

		{ // mutex scope
            std::scoped_lock lock(windowsMutex);
            for (auto& w : windows) {
                if (w) w->running = false;
            }
        }

        for (auto& [hwnd, th] : gThreads) {
            if (th.joinable()) th.join();
        }
        gThreads.clear();

        if (s_activeInstance == this) {
            s_activeInstance = nullptr;
        }
    }

    void MultiContextManager::RenderLoop(WindowData& win) {
        auto ctx = win.context;
        if (!ctx) {
            win.running = false;
            return;
        }

        ctx->windowData = &win;
       // win->commandQueue = &win.commandQueue;
        SetCurrent(ctx);

        // RAII guard: reset current context when this loop exits
        struct ResetGuard {
            ~ResetGuard() { MultiContextManager::SetCurrent(nullptr); }
        } resetGuard;

        // Backend-specific initialize
        if (ctx->initialize) {
            ctx->initialize_safe();
        }
        else {
            win.running = false;
            return;
        }

        // Main per-window loop
        while (running && win.running) {
            bool keepRunning = true;

            if (ctx->process) {
                // If the backend has its own process callback (e.g. OpenGL),
                // let it run and decide if this window stays alive.
                keepRunning = ctx->process_safe(ctx, win.commandQueue);
            }
            else {
                // Generic path — just drain the command queue
                win.commandQueue.drain();
            }

            if (!keepRunning) {
                win.running = false;
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }

        // Cleanup
        if (ctx->cleanup) {
            ctx->cleanup_safe();
        }
    }

    void MultiContextManager::HandleDropFiles(HWND hwnd, HDROP hDrop) {
        UINT count = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < count; ++i) {
            wchar_t path[MAX_PATH]{};
            DragQueryFile(hDrop, i, path, MAX_PATH);
            std::wcout << L"[Drop] " << path << L"\n";
        }
    }

    LRESULT CALLBACK almondnamespace::core::MultiContextManager::ParentProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) 
    {
        if (msg == WM_NCCREATE) 
        {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            SetWindowLongPtr(hwnd, GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return TRUE;
        }
        auto* mgr = reinterpret_cast<MultiContextManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (!mgr) return DefWindowProc(hwnd, msg, wParam, lParam);

        switch (msg) 
        {
            case WM_SIZE: 
            {
                mgr->ArrangeDockedWindowsGrid(); 
                return 0;
            }
            case WM_PAINT: 
            {
                PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
                FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
                EndPaint(hwnd, &ps); return 0;
            }
            case WM_DROPFILES:
            {
                mgr->HandleDropFiles(hwnd, reinterpret_cast<HDROP>(wParam));
                DragFinish(reinterpret_cast<HDROP>(wParam));
                return 0;
            }
            case WM_CLOSE:
            {
                DestroyWindow(hwnd); return 0;
            }
            case WM_DESTROY:
            {
                if (hwnd == mgr->GetParentWindow()) {
                    PostQuitMessage(0);
                }
                return 0;
            }

            default: 
            {
                return DefWindowProc(hwnd, msg, wParam, lParam);
            }
        }
    }

    LRESULT CALLBACK MultiContextManager::ChildProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) 
    {
        static DragState& drag = gDragState;
        if (msg == WM_NCCREATE) 
        {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return TRUE;
        }

        switch (msg) 
        {
            case WM_LBUTTONDOWN: 
            {
                SetCapture(hwnd);
                drag.dragging = true;
                drag.draggedWindow = hwnd;
                drag.originalParent = GetParent(hwnd);
                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ClientToScreen(hwnd, &pt);
                drag.lastMousePos = pt;
                return 0;
            }
            case WM_MOUSEMOVE: 
            {
                if (!drag.dragging || drag.draggedWindow != hwnd)
                    return DefWindowProc(hwnd, msg, wParam, lParam);

                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ClientToScreen(hwnd, &pt);
                int dx = pt.x - drag.lastMousePos.x;
                int dy = pt.y - drag.lastMousePos.y;
                drag.lastMousePos = pt;

                RECT wndRect; GetWindowRect(hwnd, &wndRect);
                int newX = wndRect.left + dx;
                int newY = wndRect.top + dy;

                if (drag.originalParent) {
                    RECT prc; GetClientRect(drag.originalParent, &prc);
                    POINT tl{ 0, 0 }; ClientToScreen(drag.originalParent, &tl);
                    OffsetRect(&prc, tl.x, tl.y);

                    bool inside =
                        newX >= prc.left && newY >= prc.top &&
                        (newX + (wndRect.right - wndRect.left)) <= prc.right &&
                        (newY + (wndRect.bottom - wndRect.top)) <= prc.bottom;

                    if (inside) {
                        if (GetParent(hwnd) != drag.originalParent) {
                            LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
                            style &= ~(WS_POPUP | WS_OVERLAPPEDWINDOW);
                            style |= WS_CHILD;
                            SetWindowLongPtr(hwnd, GWL_STYLE, style);
                            SetParent(hwnd, drag.originalParent);

                            POINT cp{ newX, newY };
                            ScreenToClient(drag.originalParent, &cp);
                            SetWindowPos(hwnd, nullptr, cp.x, cp.y,
                                wndRect.right - wndRect.left,
                                wndRect.bottom - wndRect.top,
                                SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                        }
                        else {
                            POINT cp{ newX, newY };
                            ScreenToClient(drag.originalParent, &cp);
                            SetWindowPos(hwnd, nullptr, cp.x, cp.y,
                                0, 0, SWP_NOZORDER | SWP_NOSIZE |
                                SWP_NOACTIVATE);
                        }
                    }
                    else 
                    {
                        if (GetParent(hwnd) == drag.originalParent) {
                            LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
                            style &= ~WS_CHILD;
                            style |= WS_OVERLAPPEDWINDOW | WS_VISIBLE;
                            SetWindowLongPtr(hwnd, GWL_STYLE, style);
                            SetParent(hwnd, nullptr);
                            SetWindowPos(hwnd, nullptr, newX, newY,
                                wndRect.right - wndRect.left,
                                wndRect.bottom - wndRect.top,
                                SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                        }
                        else {
                            SetWindowPos(hwnd, nullptr, newX, newY,
                                0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
                        }
                    }
                }
                else {
                    SetWindowPos(hwnd, nullptr, newX, newY,
                        0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
                }
                return 0;
            }
            case WM_SIZE: 
            {
                if (wParam == SIZE_MINIMIZED) 
                {
                    return 0;
                }
                auto* mgr = s_activeInstance;
                if (!mgr) {
                    break;
                }
                int width = std::max(1, static_cast<int>(LOWORD(lParam)));
                int height = std::max(1, static_cast<int>(HIWORD(lParam)));
                mgr->HandleResize(hwnd, width, height);
                return 0;
            }
            case WM_LBUTTONUP: 
            {
                if (drag.dragging && drag.draggedWindow == hwnd) {
                    ReleaseCapture();
                    drag.dragging = false;
                    drag.draggedWindow = nullptr;
                    drag.originalParent = nullptr;
                }
                return 0;
            }
            case WM_DROPFILES: 
            {
                if (HWND p = GetParent(hwnd))
                    SendMessage(p, WM_DROPFILES, wParam, lParam);
                DragFinish(reinterpret_cast<HDROP>(wParam));
                return 0;
            }
            case WM_PAINT: 
            {
                PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
                FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
                EndPaint(hwnd, &ps);
                return 0;
            }
            default:
            {
                return DefWindowProc(hwnd, msg, wParam, lParam);
            }
        }

	    return 0;
    }
} // namespace almondnamespace::core

