
#include "pch.h"

#if defined(__linux__)

#include "acontextmultiplexer.hpp"
//#include "acontext.hpp"
#include "astringconverter.hpp"

#if defined(ALMOND_USING_OPENGL)
#include "aopenglcontext.hpp"
#include "aopenglplatform.hpp"
#endif
#if defined(ALMOND_USING_RAYLIB)
#include "araylibcontext.hpp"
#endif
#if defined(ALMOND_USING_SDL)
#include "asdlcontext.hpp"
#endif
#if defined(ALMOND_USING_SOFTWARE_RENDERER)
#include "asoftrenderer_context.hpp"
#endif

#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <GL/glxext.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>

namespace almondnamespace::platform
{
    Display* global_display = nullptr;
    ::Window global_window = 0;
}

namespace almondnamespace::core
{
    using almondnamespace::platform::global_display;
    using almondnamespace::platform::global_window;

    MultiContextManager* GetActiveMultiContextManager() noexcept
    {
        return MultiContextManager::s_activeInstance;
    }

    void HandleX11Configure(::Window window, int width, int height)
    {
        if (auto* mgr = GetActiveMultiContextManager())
        {
            HWND hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(window));
            mgr->HandleResize(hwnd, width, height);
        }
    }

    namespace
    {
        constexpr int kDefaultWidth = 800;
        constexpr int kDefaultHeight = 600;

        std::once_flag g_xlibInitFlag;
        bool g_xlibInitialized = false;

        inline ::Window to_xwindow(HWND handle)
        {
            return static_cast<::Window>(reinterpret_cast<uintptr_t>(handle));
        }

        inline HWND from_xwindow(::Window window)
        {
            return reinterpret_cast<HWND>(static_cast<uintptr_t>(window));
        }

        inline Display* to_display(HDC dc)
        {
            return reinterpret_cast<Display*>(dc);
        }

        inline HDC from_display(Display* display)
        {
            return reinterpret_cast<HDC>(display);
        }

        inline GLXContext to_glx(HGLRC ctx)
        {
            return reinterpret_cast<GLXContext>(ctx);
        }

        inline HGLRC from_glx(GLXContext ctx)
        {
            return reinterpret_cast<HGLRC>(ctx);
        }

        std::wstring_view BackendDisplayName(ContextType type) noexcept
        {
            using enum ContextType;
            switch (type)
            {
            case OpenGL:   return L"OpenGL";
            case SDL:      return L"SDL";
            case SFML:     return L"SFML";
            case RayLib:   return L"Raylib";
            case Software: return L"Software";
            case Vulkan:   return L"Vulkan";
            case DirectX:  return L"DirectX";
            case Noop:     return L"Noop";
            case Custom:   return L"Custom";
            default:       return L"Context";
            }
        }

        std::wstring BuildWindowTitle(ContextType type, int index)
        {
            const std::wstring_view base = BackendDisplayName(type);
            std::wstring title{ base.begin(), base.end() };
            title += L" Dock ";
            title += std::to_wstring(static_cast<long long>(index) + 1);
            return title;
        }

        void UpdateContextDimensions(Context& ctx, WindowData& win, int width, int height)
        {
            ctx.width = width;
            ctx.height = height;
            ctx.framebufferWidth = width;
            ctx.framebufferHeight = height;
            ctx.virtualWidth = width;
            ctx.virtualHeight = height;

            win.width = width;
            win.height = height;
        }

        void SetupResizeCallback(WindowData& win)
        {
            if (win.context)
            {
                if (!win.context->onResize && win.onResize)
                {
                    win.context->onResize = win.onResize;
                }
                if (!win.onResize && win.context->onResize)
                {
                    win.onResize = win.context->onResize;
                }
            }
        }
    } // namespace

    void MultiContextManager::ShowConsole()
    {
        // No-op on Linux; console is already attached.
    }

    bool MultiContextManager::Initialize(
        HINSTANCE /*hInst*/,
        int RayLibWinCount,
        int SDLWinCount,
        int SFMLWinCount,
        int OpenGLWinCount,
        int SoftwareWinCount,
        bool /*parented*/)
    {
        const int totalRequested = RayLibWinCount + SDLWinCount + SFMLWinCount + OpenGLWinCount + SoftwareWinCount;
        if (totalRequested <= 0)
        {
            return false;
        }

        std::call_once(g_xlibInitFlag, []()
        {
            g_xlibInitialized = (XInitThreads() != 0);
        });

        if (!g_xlibInitialized)
        {
            std::cerr << "[Init] XInitThreads failed; aborting X11 initialization" << std::endl;
            return false;
        }

        s_activeInstance = this;
        running.store(true, std::memory_order_release);

        display = XOpenDisplay(nullptr);
        if (!display)
        {
            std::cerr << "[Init] Failed to open X display" << std::endl;
            return false;
        }

        screen = DefaultScreen(display);
        global_display = display;

        int fbCount = 0;
        static int visualAttribs[] = {
            GLX_X_RENDERABLE, True,
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
            GLX_DOUBLEBUFFER, True,
            GLX_RED_SIZE, 8,
            GLX_GREEN_SIZE, 8,
            GLX_BLUE_SIZE, 8,
            GLX_ALPHA_SIZE, 8,
            GLX_DEPTH_SIZE, 24,
            0
        };

        GLXFBConfig* configs = glXChooseFBConfig(display, screen, visualAttribs, &fbCount);
        if (!configs || fbCount == 0)
        {
            std::cerr << "[Init] glXChooseFBConfig failed" << std::endl;
            if (configs) XFree(configs);
            return false;
        }
        fbConfig = configs[0];
        XFree(configs);

        if (auto* vi = glXGetVisualFromFBConfig(display, fbConfig))
        {
            visualInfo = *vi;
            XFree(vi);
        }
        else
        {
            std::cerr << "[Init] glXGetVisualFromFBConfig failed" << std::endl;
            return false;
        }

        colormap = XCreateColormap(display, RootWindow(display, screen), visualInfo.visual, AllocNone);
        if (!colormap)
        {
            std::cerr << "[Init] Failed to create X colormap" << std::endl;
            return false;
        }

        wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);

        InitializeAllContexts();

        auto create_backend_windows = [&](ContextType type, int count)
        {
            if (count <= 0)
            {
                return;
            }

            std::vector<::Window> created;
            created.reserve(static_cast<size_t>(count));
            std::vector<std::string> narrowTitles;
            narrowTitles.reserve(static_cast<size_t>(count));

            for (int i = 0; i < count; ++i)
            {
                const std::wstring titleWide = BuildWindowTitle(type, i);
                const std::string titleNarrow = almondnamespace::text::narrow_utf8(titleWide);

                XSetWindowAttributes swa{};
                swa.colormap = colormap;
                swa.event_mask = ExposureMask | StructureNotifyMask |
                    KeyPressMask | KeyReleaseMask |
                    ButtonPressMask | ButtonReleaseMask |
                    PointerMotionMask | FocusChangeMask;

                ::Window win = XCreateWindow(
                    display,
                    RootWindow(display, screen),
                    0,
                    0,
                    kDefaultWidth,
                    kDefaultHeight,
                    0,
                    visualInfo.depth,
                    InputOutput,
                    visualInfo.visual,
                    CWColormap | CWEventMask,
                    &swa);

                if (!win)
                {
                    std::cerr << "[Init] Failed to create X11 window" << std::endl;
                    continue;
                }

                XStoreName(display, win, titleNarrow.c_str());
                XSetWMProtocols(display, win, &wmDeleteMessage, 1);
                XMapWindow(display, win);

                if (global_window == 0)
                {
                    global_window = win;
                }

                GLXContext glxCtx = nullptr;
                if (type == ContextType::OpenGL)
                {
                    glxCtx = CreateGLXContext();
                    if (!glxCtx)
                    {
                        XDestroyWindow(display, win);
                        continue;
                    }
                }

                auto winPtr = std::make_unique<WindowData>(
                    from_xwindow(win),
                    from_display(display),
                    from_glx(glxCtx),
                    true,
                    type);

                winPtr->titleWide = titleWide;
                winPtr->titleNarrow = titleNarrow;
                winPtr->width = kDefaultWidth;
                winPtr->height = kDefaultHeight;

                WindowData* raw = winPtr.get();
                {
                    std::scoped_lock lock(windowsMutex);
                    windows.emplace_back(std::move(winPtr));
                }

                created.push_back(win);
                narrowTitles.push_back(titleNarrow);
            }

            std::vector<std::shared_ptr<Context>> contexts;
            {
                std::unique_lock lock(g_backendsMutex);
                auto it = g_backends.find(type);
                if (it == g_backends.end() || !it->second.master)
                {
                    std::cerr << "[Init] Missing prototype context for backend type "
                              << static_cast<int>(type) << '\n';
                    return;
                }

                BackendState& state = it->second;
                contexts.reserve(static_cast<size_t>(count));
                contexts.push_back(state.master);

                auto ensure_duplicate = [&](size_t index) -> std::shared_ptr<Context>
                {
                    if (index < state.duplicates.size())
                    {
                        auto& candidate = state.duplicates[index];
                        if (candidate && candidate->initialize)
                        {
                            return candidate;
                        }
                        candidate = CloneContext(*state.master);
                        return candidate;
                    }
                    auto dup = CloneContext(*state.master);
                    state.duplicates.push_back(dup);
                    return dup;
                };

                while (contexts.size() < static_cast<size_t>(count))
                {
                    contexts.push_back(ensure_duplicate(contexts.size() - 1));
                }
            }

            const size_t limit = (std::min)(created.size(), contexts.size());
            for (size_t i = 0; i < limit; ++i)
            {
                ::Window win = created[i];
                HWND hwnd = from_xwindow(win);
                auto* window = findWindowByHWND(hwnd);
                auto& ctx = contexts[i];
                if (!window || !ctx)
                {
                    continue;
                }

                ctx->type = type;
                ctx->hwnd = hwnd;
                ctx->hdc = window->hdc;
                ctx->hglrc = window->glContext;
                ctx->windowData = window;
                window->context = ctx;

                XWindowAttributes attrs{};
                if (XGetWindowAttributes(display, win, &attrs))
                {
                    UpdateContextDimensions(*ctx, *window, (std::max)(1, attrs.width), (std::max)(1, attrs.height));
                }
                else
                {
                    UpdateContextDimensions(*ctx, *window, kDefaultWidth, kDefaultHeight);
                }

                SetupResizeCallback(*window);

#if defined(ALMOND_USING_OPENGL)
                if (type == ContextType::OpenGL)
                {
                    const unsigned width = static_cast<unsigned>((std::max)(1, window->width));
                    const unsigned height = static_cast<unsigned>((std::max)(1, window->height));
                    auto resizeCopy = window->onResize;

                    window->threadInitialize = [ctxWeak = std::weak_ptr<Context>(ctx),
                        width,
                        height,
                        resize = std::move(resizeCopy)](const std::shared_ptr<Context>& liveCtx) mutable -> bool
                    {
                        auto target = liveCtx ? liveCtx : ctxWeak.lock();
                        if (!target)
                        {
                            std::cerr << "[Init] OpenGL context unavailable during thread initialization\n";
                            return false;
                        }

                        try
                        {
                            if (!almondnamespace::openglcontext::opengl_initialize(
                                    target,
                                    nullptr,
                                    width,
                                    height,
                                    std::move(resize)))
                            {
                                std::cerr << "[Init] Failed to initialize OpenGL context for hwnd="
                                          << target->hwnd << '\n';
                                return false;
                            }
                        }
                        catch (const std::exception& e)
                        {
                            std::cerr << "[Init] Exception during OpenGL initialization for hwnd="
                                      << target->hwnd << ": " << e.what() << '\n';
                            return false;
                        }
                        catch (...)
                        {
                            std::cerr << "[Init] Unknown exception during OpenGL initialization for hwnd="
                                      << target->hwnd << '\n';
                            return false;
                        }

                        return true;
                    };
                }
#endif

#if defined(ALMOND_USING_SOFTWARE_RENDERER)
                if (type == ContextType::Software)
                {
                    const unsigned width = static_cast<unsigned>((std::max)(1, window->width));
                    const unsigned height = static_cast<unsigned>((std::max)(1, window->height));
                    if (!almondnamespace::anativecontext::softrenderer_initialize(
                            ctx,
                            nullptr,
                            width,
                            height,
                            window->onResize))
                    {
                        std::cerr << "[Init] Failed to initialize Software renderer for hwnd="
                                  << ctx->hwnd << '\n';
                        window->running = false;
                    }
                }
#endif
#if defined(ALMOND_USING_SDL)
                if (type == ContextType::SDL)
                {
                    const int width = (std::max)(1, window->width);
                    const int height = (std::max)(1, window->height);
                    const std::string title = (i < narrowTitles.size()) ? narrowTitles[i] : std::string("SDL Dock");
                    auto resizeCopy = window->onResize;

                    window->threadInitialize = [ctxWeak = std::weak_ptr<Context>(ctx),
                        width,
                        height,
                        title,
                        resize = std::move(resizeCopy)](const std::shared_ptr<Context>& liveCtx) mutable -> bool
                    {
                        auto target = liveCtx ? liveCtx : ctxWeak.lock();
                        if (!target)
                        {
                            std::cerr << "[Init] SDL context unavailable during thread initialization\n";
                            return false;
                        }

                        if (!almondnamespace::sdlcontext::sdl_initialize(
                                target,
                                nullptr,
                                width,
                                height,
                                std::move(resize),
                                title))
                        {
                            std::cerr << "[Init] Failed to initialize SDL context for hwnd="
                                      << target->hwnd << '\n';
                            return false;
                        }

                        return true;
                    };
                }
#endif
#if defined(ALMOND_USING_RAYLIB)
                if (type == ContextType::RayLib)
                {
                    const unsigned width = static_cast<unsigned>((std::max)(1, window->width));
                    const unsigned height = static_cast<unsigned>((std::max)(1, window->height));
                    const std::string& title = (i < narrowTitles.size()) ? narrowTitles[i] : std::string("Raylib Dock");
                    auto resizeCopy = window->onResize;

                    window->threadInitialize = [ctxWeak = std::weak_ptr<Context>(ctx),
                        width,
                        height,
                        title,
                        resize = std::move(resizeCopy)](const std::shared_ptr<Context>& liveCtx) mutable -> bool
                    {
                        auto target = liveCtx ? liveCtx : ctxWeak.lock();
                        if (!target)
                        {
                            std::cerr << "[Init] RayLib context unavailable during thread initialization\n";
                            return false;
                        }

                        if (!almondnamespace::raylibcontext::raylib_initialize(
                                target,
                                nullptr,
                                width,
                                height,
                                std::move(resize),
                                title))
                        {
                            std::cerr << "[Init] Failed to initialize RayLib context for hwnd="
                                      << target->hwnd << '\n';
                            return false;
                        }

                        // Raylib may manage its own native handles on Linux; keep
                        // the placeholder window data so the multiplexer can
                        // continue managing the X11 resources it created.
                        return true;
                    };
                }
#endif
            }
        };

        create_backend_windows(ContextType::OpenGL, OpenGLWinCount);
        create_backend_windows(ContextType::SDL, SDLWinCount);
        create_backend_windows(ContextType::SFML, SFMLWinCount);
        create_backend_windows(ContextType::RayLib, RayLibWinCount);
        create_backend_windows(ContextType::Software, SoftwareWinCount);

        StartRenderThreads();
        return true;
    }

    void MultiContextManager::StopAll()
    {
        running.store(false, std::memory_order_release);

        {
            std::scoped_lock lock(windowsMutex);
            for (auto& win : windows)
            {
                if (win)
                {
                    win->running = false;
                }
            }
        }

        for (auto& [win, thread] : threads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
        threads.clear();

        {
            std::scoped_lock lock(windowsMutex);
            for (auto& win : windows)
            {
                if (win)
                {
                    DestroyWindowData(*win);
                }
            }
            windows.clear();
        }

        if (colormap)
        {
            XFreeColormap(display, colormap);
            colormap = 0;
        }

        if (display)
        {
            if (global_display == display)
            {
                global_display = nullptr;
                global_window = 0;
            }
            XCloseDisplay(display);
            display = nullptr;
        }

        if (s_activeInstance == this)
        {
            s_activeInstance = nullptr;
        }
    }

    bool MultiContextManager::IsRunning() const noexcept
    {
        return running.load(std::memory_order_acquire);
    }

    void MultiContextManager::StopRunning() noexcept
    {
        running.store(false, std::memory_order_release);
    }

    GLXContext MultiContextManager::CreateGLXContext()
    {
        if (!display)
        {
            return nullptr;
        }

        auto* createContextAttribs = reinterpret_cast<PFNGLXCREATECONTEXTATTRIBSARBPROC>(
            glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glXCreateContextAttribsARB")));

        int contextAttribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
            GLX_CONTEXT_MINOR_VERSION_ARB, 3,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };

        GLXContext shared = sharedContext;
        GLXContext ctx = nullptr;
        if (createContextAttribs)
        {
            ctx = createContextAttribs(display, fbConfig, shared, True, contextAttribs);
        }

        if (!ctx)
        {
            ctx = glXCreateNewContext(display, fbConfig, GLX_RGBA_TYPE, shared, True);
        }

        if (!sharedContext && ctx)
        {
            sharedContext = ctx;
        }

        return ctx;
    }

    void MultiContextManager::DestroyWindowData(WindowData& win)
    {
        ::Window window = to_xwindow(win.hwnd);
        GLXContext ctx = to_glx(win.glContext);

        if (display && ctx)
        {
            glXDestroyContext(display, ctx);
            if (sharedContext == ctx)
            {
                sharedContext = nullptr;
            }
        }

        if (display && window)
        {
            XDestroyWindow(display, window);
            if (global_window == window)
            {
                global_window = 0;
            }
        }

        win.glContext = nullptr;
        win.hwnd = nullptr;
        win.hdc = nullptr;
        win.context.reset();
    }

    void MultiContextManager::AddWindow(
        HWND hwnd,
        HWND /*parent*/,
        HDC hdc,
        HGLRC glContext,
        bool usesSharedContext,
        ResizeCallback onResize,
        ContextType type)
    {
        if (!hwnd)
        {
            return;
        }

        if (!display)
        {
            display = to_display(hdc);
        }

        if (!display)
        {
            return;
        }

        ::Window window = to_xwindow(hwnd);
        Display* localDisplay = display;

        if (type == ContextType::OpenGL)
        {
            if (!glContext)
            {
                glContext = from_glx(CreateGLXContext());
            }
        }

        bool needInit = false;
        {
            std::shared_lock lock(g_backendsMutex);
            needInit = g_backends.empty();
        }
        if (needInit)
        {
            InitializeAllContexts();
        }

        std::shared_ptr<Context> ctx;
        {
            std::unique_lock lock(g_backendsMutex);
            auto& state = g_backends[type];
            if (!state.master)
            {
                ctx = std::make_shared<Context>();
                ctx->type = type;
                state.master = ctx;
            }
            else if (!state.master->windowData)
            {
                ctx = state.master;
            }
            else
            {
                auto it = std::find_if(
                    state.duplicates.begin(),
                    state.duplicates.end(),
                    [](const std::shared_ptr<Context>& dup)
                    {
                        return dup && !dup->windowData;
                    });
                if (it != state.duplicates.end())
                {
                    ctx = *it;
                }
                else
                {
                    auto dup = CloneContext(*state.master);
                    state.duplicates.push_back(dup);
                    ctx = dup;
                }
            }
        }

        if (!ctx)
        {
            return;
        }

        ctx->type = type;
        ctx->hwnd = hwnd;
        ctx->hdc = hdc ? hdc : from_display(localDisplay);
        ctx->hglrc = glContext;

        auto winPtr = std::make_unique<WindowData>(
            hwnd,
            hdc ? hdc : from_display(localDisplay),
            glContext,
            usesSharedContext,
            type);

        winPtr->onResize = std::move(onResize);
        winPtr->context = ctx;
        ctx->windowData = winPtr.get();

        XWindowAttributes attrs{};
        if (XGetWindowAttributes(localDisplay, window, &attrs))
        {
            UpdateContextDimensions(*ctx, *winPtr, (std::max)(1, attrs.width), (std::max)(1, attrs.height));
        }
        else
        {
            UpdateContextDimensions(*ctx, *winPtr, kDefaultWidth, kDefaultHeight);
        }

        SetupResizeCallback(*winPtr);

#if defined(ALMOND_USING_OPENGL)
        if (type == ContextType::OpenGL)
        {
            const unsigned width = static_cast<unsigned>((std::max)(1, winPtr->width));
            const unsigned height = static_cast<unsigned>((std::max)(1, winPtr->height));
            auto resizeCopy = winPtr->onResize;

            winPtr->threadInitialize = [ctxWeak = std::weak_ptr<Context>(ctx),
                width,
                height,
                resize = std::move(resizeCopy)](const std::shared_ptr<Context>& liveCtx) mutable -> bool
            {
                auto target = liveCtx ? liveCtx : ctxWeak.lock();
                if (!target)
                {
                    std::cerr << "[Init] OpenGL context unavailable during thread initialization\n";
                    return false;
                }

                try
                {
                    if (!almondnamespace::openglcontext::opengl_initialize(
                            target,
                            nullptr,
                            width,
                            height,
                            std::move(resize)))
                    {
                        std::cerr << "[Init] Failed to initialize OpenGL context for hwnd="
                                  << target->hwnd << '\n';
                        return false;
                    }
                }
                catch (const std::exception& e)
                {
                    std::cerr << "[Init] Exception during OpenGL initialization for hwnd="
                              << target->hwnd << ": " << e.what() << '\n';
                    return false;
                }
                catch (...)
                {
                    std::cerr << "[Init] Unknown exception during OpenGL initialization for hwnd="
                              << target->hwnd << '\n';
                    return false;
                }

                return true;
            };
        }
#endif

        WindowData* raw = winPtr.get();
        {
            std::scoped_lock lock(windowsMutex);
            windows.emplace_back(std::move(winPtr));
        }

        threads[window] = std::thread([this, raw]()
        {
            RenderLoop(*raw);
        });
    }

    void MultiContextManager::RemoveWindow(HWND hwnd)
    {
        if (!hwnd)
        {
            return;
        }

        std::unique_ptr<WindowData> removed;

        {
            std::scoped_lock lock(windowsMutex);
            auto it = std::find_if(
                windows.begin(),
                windows.end(),
                [hwnd](const std::unique_ptr<WindowData>& w)
                {
                    return w && w->hwnd == hwnd;
                });
            if (it == windows.end())
            {
                return;
            }

            (*it)->running = false;
            removed = std::move(*it);
            windows.erase(it);
        }

        ::Window window = to_xwindow(hwnd);
        auto threadIt = threads.find(window);
        if (threadIt != threads.end())
        {
            if (threadIt->second.joinable())
            {
                threadIt->second.join();
            }
            threads.erase(threadIt);
        }

        if (removed)
        {
            if (removed->context && removed->context->windowData == removed.get())
            {
                removed->context->windowData = nullptr;
            }
            DestroyWindowData(*removed);

            if (global_window == 0)
            {
                std::scoped_lock lock(windowsMutex);
                for (const auto& candidate : windows)
                {
                    if (candidate && candidate->hwnd)
                    {
                        global_window = to_xwindow(candidate->hwnd);
                        break;
                    }
                }
            }
        }
    }

    void MultiContextManager::HandleResize(HWND hwnd, int width, int height)
    {
        if (!hwnd)
        {
            return;
        }

        const int clampedWidth = (std::max)(1, width);
        const int clampedHeight = (std::max)(1, height);

        std::function<void(int, int)> resizeCallback;
        WindowData* window = nullptr;

        {
            std::scoped_lock lock(windowsMutex);
            auto it = std::find_if(
                windows.begin(),
                windows.end(),
                [hwnd](const std::unique_ptr<WindowData>& w)
                {
                    return w && w->hwnd == hwnd;
                });

            if (it == windows.end())
            {
                return;
            }

            window = it->get();
            window->width = clampedWidth;
            window->height = clampedHeight;

            if (window->context)
            {
                window->context->width = clampedWidth;
                window->context->height = clampedHeight;
                if (window->context->onResize)
                {
                    resizeCallback = window->context->onResize;
                }
            }

            if (!resizeCallback && window->onResize)
            {
                resizeCallback = window->onResize;
            }
        }

        if (resizeCallback && window)
        {
            window->commandQueue.enqueue([
                cb = std::move(resizeCallback),
                clampedWidth,
                clampedHeight
            ]() mutable
            {
                if (cb)
                {
                    cb(clampedWidth, clampedHeight);
                }
            });
        }
    }

    void MultiContextManager::StartRenderThreads()
    {
        std::scoped_lock lock(windowsMutex);
        for (const auto& win : windows)
        {
            if (!win)
            {
                continue;
            }

            ::Window window = to_xwindow(win->hwnd);
            if (!threads.contains(window))
            {
                WindowData* raw = win.get();
                threads[window] = std::thread([this, raw]()
                {
                    RenderLoop(*raw);
                });
            }
        }
    }

    void MultiContextManager::EnqueueRenderCommand(HWND hwnd, RenderCommand cmd)
    {
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(
            windows.begin(),
            windows.end(),
            [hwnd](const std::unique_ptr<WindowData>& w)
            {
                return w && w->hwnd == hwnd;
            });
        if (it != windows.end() && *it)
        {
            (*it)->EnqueueCommand(std::move(cmd));
        }
    }

    void MultiContextManager::SetCurrent(std::shared_ptr<core::Context> ctx)
    {
        currentContext = std::move(ctx);
    }

    std::shared_ptr<core::Context> MultiContextManager::GetCurrent()
    {
        return currentContext;
    }

    WindowData* MultiContextManager::findWindowByHWND(HWND hwnd)
    {
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(
            windows.begin(),
            windows.end(),
            [hwnd](const std::unique_ptr<WindowData>& w)
            {
                return w && w->hwnd == hwnd;
            });
        return (it != windows.end()) ? it->get() : nullptr;
    }

    const WindowData* MultiContextManager::findWindowByHWND(HWND hwnd) const
    {
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(
            windows.begin(),
            windows.end(),
            [hwnd](const std::unique_ptr<WindowData>& w)
            {
                return w && w->hwnd == hwnd;
            });
        return (it != windows.end()) ? it->get() : nullptr;
    }

    WindowData* MultiContextManager::findWindowByContext(const std::shared_ptr<core::Context>& ctx)
    {
        if (!ctx)
        {
            return nullptr;
        }

        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(
            windows.begin(),
            windows.end(),
            [&](const std::unique_ptr<WindowData>& w)
            {
                return w && w->context && w->context.get() == ctx.get();
            });
        return (it != windows.end()) ? it->get() : nullptr;
    }

    const WindowData* MultiContextManager::findWindowByContext(const std::shared_ptr<core::Context>& ctx) const
    {
        if (!ctx)
        {
            return nullptr;
        }

        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(
            windows.begin(),
            windows.end(),
            [&](const std::unique_ptr<WindowData>& w)
            {
                return w && w->context && w->context.get() == ctx.get();
            });
        return (it != windows.end()) ? it->get() : nullptr;
    }

    void MultiContextManager::RenderLoop(WindowData& win)
    {
        auto ctx = win.context;
        if (!ctx)
        {
            win.running = false;
            return;
        }

        Display* localDisplay = to_display(win.hdc);
        ::Window window = to_xwindow(win.hwnd);
        GLXContext glxCtx = to_glx(win.glContext);

        if (glxCtx && localDisplay)
        {
            glXMakeCurrent(localDisplay, window, glxCtx);

#if defined(ALMOND_USING_OPENGL) || defined(ALMOND_USING_RAYLIB) || defined(ALMOND_USING_SDL)
            static std::atomic<bool> gladInitialized{ false };
            if (!gladInitialized.load(std::memory_order_acquire))
            {
                auto loadProc = [](const char* name) -> void*
                {
#if defined(ALMOND_USING_OPENGL)
                    return almondnamespace::openglcontext::PlatformGL::get_proc_address(name);
#else
                    return reinterpret_cast<void*>(
                        glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(name)));
#endif
                };

                if (gladLoadGLLoader(loadProc))
                {
                    gladInitialized.store(true, std::memory_order_release);
                }
                else
                {
                    std::cerr << "[Init] Failed to load OpenGL functions via GLAD on Linux\n";
                }
            }
#endif
        }

        ctx->windowData = &win;
        SetCurrent(ctx);

        struct ResetGuard
        {
            Display* display{};
            GLXContext ctx{};
            ~ResetGuard()
            {
                if (display && ctx)
                {
                    glXMakeCurrent(display, 0, nullptr);
                }
                MultiContextManager::SetCurrent(nullptr);
            }
        } reset{ localDisplay, glxCtx };

        if (win.threadInitialize)
        {
            auto init = std::move(win.threadInitialize);
            win.threadInitialize = nullptr;
            if (!init || !init(ctx))
            {
                win.running = false;
                return;
            }
        }

        if (ctx->initialize)
        {
            ctx->initialize_safe();
        }
        else
        {
            win.running = false;
            return;
        }

        while (running.load(std::memory_order_acquire) && win.running)
        {
            bool keepRunning = true;
            if (ctx->process)
            {
                keepRunning = ctx->process_safe(ctx, win.commandQueue);
            }
            else
            {
                win.commandQueue.drain();
            }

            if (!keepRunning)
            {
                win.running = false;
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        if (ctx->cleanup)
        {
            ctx->cleanup_safe();
        }
    }

} // namespace almondnamespace::core

#endif // defined(__linux__)
