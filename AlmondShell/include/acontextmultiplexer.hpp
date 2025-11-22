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
 // acontextmultiplexer.hpp
#pragma once

#include "aplatform.hpp"        // Must always come first for platform defines
#include "aengineconfig.hpp"   // All ENGINE-specific includes

#include "acontext.hpp"       // Context, ContextType
#include "acontexttype.hpp"
#include "awindowdata.hpp"

#if defined(_WIN32)
//#    include <windows.h>
//#    include <windowsx.h>
//#    include <shellapi.h>
//#    include <commctrl.h>
//#    pragma comment(lib, "comctl32.lib")
#else
#    include <cstdint>
struct POINT { long x{}; long y{}; };
using HINSTANCE = void*;
using HDROP = void*;
using ATOM = unsigned int;
using LRESULT = long;
using UINT = unsigned int;
using WPARAM = uintptr_t;
using LPARAM = intptr_t;
using LPCWSTR = const wchar_t*;
using UINT_PTR = uintptr_t;
using DWORD_PTR = uintptr_t;
#    ifndef CALLBACK
#        define CALLBACK
#    endif
#endif

#if defined(__linux__)
#    include <X11/Xlib.h>
#    include <X11/Xutil.h>
#    include <GL/glx.h>
#endif

#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <unordered_map>

//#if defined(ALMOND_USING_OPENGL) || defined(ALMOND_USING_RAYLIB) || defined(ALMOND_USING_SDL)
//#include <glad/glad.h>
//#endif

//#define ALMOND_SINGLE_PARENT 1  // 0 = multiple top-level windows, 1 = single parent + children
#define ALMOND_SHARED_CONTEXT 1  // Force shared OpenGL context

#if !defined(ALMOND_SINGLE_PARENT) || (ALMOND_SINGLE_PARENT == 0)
#undef ALMOND_SHARED_CONTEXT
#define ALMOND_SHARED_CONTEXT 0
#else
#define ALMOND_USING_DOCKING 1  // Enable docking features
#define ALMOND_SHARED_CONTEXT 1 // Enable shared OpenGL context
#endif

namespace almondnamespace::core
{
    // -----------------------------------------------------------------
    // Global thread table (key = HWND)
    // -----------------------------------------------------------------
#if defined(_WIN32)
    extern std::unordered_map<HWND, std::thread> gThreads;

    // -----------------------------------------------------------------
    // Drag state (for child window docking/movement)
    // -----------------------------------------------------------------
    struct DragState {
        bool dragging = false;
        POINT lastMousePos{};
        HWND draggedWindow = nullptr;
        HWND originalParent = nullptr;
    };
    extern DragState gDragState;

    void MakeDockable(HWND hwnd, HWND parent);
#endif

    // ======================================================
    // MultiContextManager : Main orchestrator
    // ======================================================
    struct WindowData; // Forward declaration
    struct CommandQueue; // Forward declaration

#if defined(_WIN32)
    class MultiContextManager
    {
    public:
        // ---- Lifecycle ----
        static void ShowConsole();
        bool Initialize(HINSTANCE hInst,
            int RayLibWinCount = 0,
            int SDLWinCount = 0,
            int SFMLWinCount = 0,
            int OpenGLWinCount = 0,
            int SoftwareWinCount = 0,
            bool parented = ALMOND_SINGLE_PARENT == 1);

        void StopAll();
        bool IsRunning() const noexcept;
        void StopRunning() noexcept;

        // ---- Window / Context Management ----
        using ResizeCallback = std::function<void(int, int)>;
        void AddWindow(HWND hwnd, HWND parent, HDC hdc, HGLRC glContext,
            bool usesSharedContext,
            ResizeCallback onResize,
            ContextType type);
        void RemoveWindow(HWND hwnd);
        void ArrangeDockedWindowsGrid();
        void HandleResize(HWND hwnd, int width, int height);
        void StartRenderThreads();

        HWND GetParentWindow() const { return parent; }
        const std::vector<std::unique_ptr<WindowData>>& GetWindows() const { return windows; }

        // ---- Render Commands ----
        using RenderCommand = std::function<void()>;
        void EnqueueRenderCommand(HWND hwnd, MultiContextManager::RenderCommand cmd);

        // ---- Context API ----
        static void SetCurrent(std::shared_ptr<core::Context> ctx);
        static std::shared_ptr<core::Context> GetCurrent();

        // ---- Windows Message Handling ----
        static LRESULT CALLBACK ParentProc(HWND, UINT, WPARAM, LPARAM);
        static LRESULT CALLBACK ChildProc(HWND, UINT, WPARAM, LPARAM);
        void HandleDropFiles(HWND, HDROP);

        static ATOM RegisterParentClass(HINSTANCE, LPCWSTR);
        static ATOM RegisterChildClass(HINSTANCE, LPCWSTR);

        WindowData* findWindowByHWND(HWND hwnd);
        const WindowData* findWindowByHWND(HWND hwnd) const;
        WindowData* findWindowByContext(const std::shared_ptr<core::Context>& ctx);
        const WindowData* findWindowByContext(const std::shared_ptr<core::Context>& ctx) const;
    private:
        // ---- Internal State ----
        std::vector<std::unique_ptr<WindowData>> windows;
        std::atomic<bool> running{ true };
        mutable std::mutex windowsMutex;

        HGLRC sharedContext = nullptr;
        HWND  parent = nullptr;

        // ---- Internals ----
        void RenderLoop(WindowData& win);
        void SetupPixelFormat(HDC hdc);
        HGLRC CreateSharedGLContext(HDC hdc);
        int get_title_bar_thickness(const HWND window_handle);

        // ---- Static Shared ----
        inline static thread_local std::shared_ptr<core::Context> currentContext;
        inline static MultiContextManager* s_activeInstance = nullptr;
    };
#elif defined(__linux__)
    class MultiContextManager
    {
    public:
        using ResizeCallback = std::function<void(int, int)>;
        using RenderCommand = std::function<void()>;

        static void ShowConsole();
        bool Initialize(HINSTANCE hInst,
            int RayLibWinCount = 0,
            int SDLWinCount = 0,
            int SFMLWinCount = 0,
            int OpenGLWinCount = 0,
            int SoftwareWinCount = 0,
            bool parented = false);

        void StopAll();
        bool IsRunning() const noexcept;
        void StopRunning() noexcept;

        void AddWindow(HWND hwnd, HWND parent, HDC hdc, HGLRC glContext,
            bool usesSharedContext,
            ResizeCallback onResize,
            ContextType type);
        void RemoveWindow(HWND hwnd);
        void ArrangeDockedWindowsGrid() {}
        void HandleResize(HWND hwnd, int width, int height);
        void StartRenderThreads();

        HWND GetParentWindow() const { return nullptr; }
        const std::vector<std::unique_ptr<WindowData>>& GetWindows() const { return windows; }

        void EnqueueRenderCommand(HWND hwnd, RenderCommand cmd);

        static void SetCurrent(std::shared_ptr<core::Context> ctx);
        static std::shared_ptr<core::Context> GetCurrent();

        WindowData* findWindowByHWND(HWND hwnd);
        const WindowData* findWindowByHWND(HWND hwnd) const;
        WindowData* findWindowByContext(const std::shared_ptr<core::Context>& ctx);
        const WindowData* findWindowByContext(const std::shared_ptr<core::Context>& ctx) const;

    private:
        void RenderLoop(WindowData& win);
        GLXContext CreateGLXContext();
        void DestroyWindowData(WindowData& win);

        std::vector<std::unique_ptr<WindowData>> windows;
        std::unordered_map<::Window, std::thread> threads;
        std::atomic<bool> running{ true };
        mutable std::mutex windowsMutex;

        Display* display = nullptr;
        int screen = 0;
        GLXFBConfig fbConfig = nullptr;
        Colormap colormap = 0;
        GLXContext sharedContext = nullptr;
        Atom wmDeleteMessage = 0;
        XVisualInfo visualInfo{};

        inline static thread_local std::shared_ptr<core::Context> currentContext{};
        inline static MultiContextManager* s_activeInstance = nullptr;

        friend MultiContextManager* GetActiveMultiContextManager() noexcept;
        friend void HandleX11Configure(::Window window, int width, int height);
    };
#else
    class MultiContextManager
    {
    public:
        using ResizeCallback = std::function<void(int, int)>;
        using RenderCommand = std::function<void()>;

        static void ShowConsole() {}
        bool Initialize(HINSTANCE, int, int, int, int, int, bool) { return false; }
        void StopAll() {}
        bool IsRunning() const noexcept { return false; }
        void StopRunning() noexcept {}

        void AddWindow(HWND, HWND, HDC, HGLRC, bool, ResizeCallback, ContextType) {}
        void RemoveWindow(HWND) {}
        void ArrangeDockedWindowsGrid() {}
        void StartRenderThreads() {}

        void HandleResize(HWND, int, int) {}

        HWND GetParentWindow() const { return nullptr; }
        const std::vector<std::unique_ptr<WindowData>>& GetWindows() const { return s_emptyWindows; }

        void EnqueueRenderCommand(HWND, RenderCommand) {}

        static void SetCurrent(std::shared_ptr<core::Context> ctx) { s_currentContext = std::move(ctx); }
        static std::shared_ptr<core::Context> GetCurrent() { return s_currentContext; }

        static LRESULT CALLBACK ParentProc(HWND, UINT, WPARAM, LPARAM) { return 0; }
        static LRESULT CALLBACK ChildProc(HWND, UINT, WPARAM, LPARAM) { return 0; }
        void HandleDropFiles(HWND, HDROP) {}

        WindowData* findWindowByHWND(HWND) { return nullptr; }
        const WindowData* findWindowByHWND(HWND) const { return nullptr; }
        WindowData* findWindowByContext(const std::shared_ptr<core::Context>&) { return nullptr; }
        const WindowData* findWindowByContext(const std::shared_ptr<core::Context>&) const { return nullptr; }

    private:
        inline static const std::vector<std::unique_ptr<WindowData>> s_emptyWindows{};
        inline static thread_local std::shared_ptr<core::Context> s_currentContext{};
    };
#endif

#if defined(__linux__)
    MultiContextManager* GetActiveMultiContextManager() noexcept;
    void HandleX11Configure(::Window window, int width, int height);
#endif

} // namespace almondnamespace::core
