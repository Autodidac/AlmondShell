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
 // araylibcontext.hpp
#pragma once

#include "aplatform.hpp"
#include "aengineconfig.hpp"

#if defined(ALMOND_USING_RAYLIB)

#include "acontext.hpp"
#include "awindowdata.hpp"

#include "acontextwindow.hpp"
#include "arobusttime.hpp"
#include "aimageloader.hpp"
#include "araylibtextures.hpp" // AtlasGPU, gpu_atlases, ensure_uploaded
#include "araylibstate.hpp"    // brings in the actual inline s_raylibstate
#include "aatlasmanager.hpp"
#include "araylibrenderer.hpp" // RendererContext, RenderMode
#include "acommandline.hpp"    // cli::window_width, cli::window_height
#include "araylibcontextinput.hpp"

#if defined(_WIN32)
namespace almondnamespace::core { void MakeDockable(HWND hwnd, HWND parent); }
#endif

#include <algorithm>
#include <cassert>
#include <cmath>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

// Raylib last
#include <raylib.h>
#if !defined(RAYLIB_NO_WINDOW)
extern "C" void CloseWindow(void);
#endif

#if defined(_WIN32)
//#include <windows.h>
#include <gl/gl.h>
#endif

namespace almondnamespace::raylibcontext
{
#if defined(_WIN32)
    using NativeWindowHandle = HWND;
    using NativeDeviceContext = HDC;
    using NativeGlContext = HGLRC;
#else
    using NativeWindowHandle = void*;
    using NativeDeviceContext = void*;
    using NativeGlContext = void*;
#endif

    namespace detail
    {
#if defined(_WIN32)
        inline NativeDeviceContext current_dc() noexcept { return ::wglGetCurrentDC(); }
        inline NativeGlContext current_context() noexcept { return ::wglGetCurrentContext(); }
        inline bool make_current(NativeDeviceContext dc, NativeGlContext ctx) noexcept
        {
            if (!dc || !ctx) return false;
            return ::wglMakeCurrent(dc, ctx) == TRUE;
        }
        inline void clear_current() noexcept { ::wglMakeCurrent(nullptr, nullptr); }

        inline bool refresh_wgl_handles()
        {
            const auto currentWindow = static_cast<HWND>(GetWindowHandle());
            const bool windowChanged = currentWindow && currentWindow != s_raylibstate.hwnd;

            if (!windowChanged && s_raylibstate.hdc && s_raylibstate.glContext) {
                if (make_current(s_raylibstate.hdc, s_raylibstate.glContext)) {
                    return true;
                }
            }

            const HWND previousWindow = s_raylibstate.hwnd;
            const HDC previousDC = s_raylibstate.hdc;
            const HGLRC previousContext = s_raylibstate.glContext;

            if (s_raylibstate.ownsDC && s_raylibstate.hdc && s_raylibstate.hwnd) {
                ::ReleaseDC(s_raylibstate.hwnd, s_raylibstate.hdc);
            }

            s_raylibstate.hwnd = currentWindow;
            s_raylibstate.hdc = nullptr;
            s_raylibstate.glContext = nullptr;
            s_raylibstate.ownsDC = false;

            if (!currentWindow) {
                return false;
            }

            HDC candidateDC = ::GetDC(currentWindow);
            if (!candidateDC) {
                return false;
            }

            const HDC activeDC = current_dc();
            if (activeDC && activeDC != candidateDC) {
                ::ReleaseDC(currentWindow, candidateDC);
                candidateDC = activeDC;
                s_raylibstate.ownsDC = false;
            }
            else {
                s_raylibstate.ownsDC = true;
            }

            const HGLRC ctx = current_context();
            if (!ctx) {
                if (s_raylibstate.ownsDC && candidateDC) {
                    ::ReleaseDC(currentWindow, candidateDC);
                }
                s_raylibstate.ownsDC = false;
                return false;
            }

            if (!make_current(candidateDC, ctx)) {
                if (s_raylibstate.ownsDC && candidateDC) {
                    ::ReleaseDC(currentWindow, candidateDC);
                }
                s_raylibstate.ownsDC = false;
                return false;
            }

            s_raylibstate.hdc = candidateDC;
            s_raylibstate.glContext = ctx;

            if (currentWindow != previousWindow || candidateDC != previousDC
                || ctx != previousContext) {
                std::clog << "[Raylib] Refreshed GL handles (hwnd=" << s_raylibstate.hwnd
                          << ", hdc=" << s_raylibstate.hdc
                          << ", hglrc=" << s_raylibstate.glContext << ")\n";
            }

            return true;
        }
#else
        inline NativeDeviceContext current_dc() noexcept { return nullptr; }
        inline NativeGlContext current_context() noexcept { return nullptr; }
        inline bool make_current(NativeDeviceContext, NativeGlContext) noexcept { return true; }
        inline void clear_current() noexcept {}
        inline bool refresh_wgl_handles() noexcept { return true; }
#endif

        inline bool contexts_match(NativeDeviceContext lhsDC,
            NativeGlContext lhsCtx,
            NativeDeviceContext rhsDC,
            NativeGlContext rhsCtx) noexcept
        {
#if defined(_WIN32)
            return lhsDC == rhsDC && lhsCtx == rhsCtx;
#else
            (void)lhsDC;
            (void)lhsCtx;
            (void)rhsDC;
            (void)rhsCtx;
            return true;
#endif
        }
    } // namespace detail

    // Usually at the start of your program, before window creation:
    static almondnamespace::contextwindow::WindowData* g_raylibwindowContext;

    inline GuiFitViewport compute_fit_viewport(int fbW, int fbH, int refW, int refH) noexcept
    {
        GuiFitViewport r{};
        r.fbW = (std::max)(1, fbW);
        r.fbH = (std::max)(1, fbH);
        r.refW = (std::max)(1, refW);
        r.refH = (std::max)(1, refH);

        const float sx = float(r.fbW) / float(r.refW);
        const float sy = float(r.fbH) / float(r.refH);
        r.scale = (std::max)(0.0001f, (std::min)(sx, sy)); // fit

        r.vpW = (std::max)(1, int(std::lround(r.refW * r.scale)));
        r.vpH = (std::max)(1, int(std::lround(r.refH * r.scale)));
        r.vpX = (r.fbW - r.vpW) / 2;
        r.vpY = (r.fbH - r.vpH) / 2;
        return r;
    }

    inline void raylib_get_mouse_position(int& x, int& y) noexcept
    {
        Vector2 mousePos{ 0.0f, 0.0f };
#if !defined(RAYLIB_NO_WINDOW)
        if (IsWindowReady())
            mousePos = GetMousePosition();
#else
        mousePos = GetMousePosition();
#endif

        const int resolvedX = static_cast<int>(std::lround(mousePos.x));
        const int resolvedY = static_cast<int>(std::lround(mousePos.y));

        const int maxW = static_cast<int>((std::max)(1u, s_raylibstate.virtualWidth));
        const int maxH = static_cast<int>((std::max)(1u, s_raylibstate.virtualHeight));
        x = std::clamp(resolvedX, -1, maxW);
        y = std::clamp(resolvedY, -1, maxH);
    }

    inline void seed_viewport_from_framebuffer(const std::shared_ptr<core::Context>& ctx) noexcept
    {
        const bool widthOverride = core::cli::window_width_overridden
            && core::cli::window_width > 0;
        const bool heightOverride = core::cli::window_height_overridden
            && core::cli::window_height > 0;

        int refW = widthOverride ? core::cli::window_width
            : static_cast<int>(s_raylibstate.virtualWidth);
        int refH = heightOverride ? core::cli::window_height
            : static_cast<int>(s_raylibstate.virtualHeight);

        if (ctx) {
            if (ctx->width > 0) refW = ctx->width;
            if (ctx->height > 0) refH = ctx->height;
        }

        int fbW = static_cast<int>(s_raylibstate.width);
        int fbH = static_cast<int>(s_raylibstate.height);

#if !defined(RAYLIB_NO_WINDOW)
        if (::IsWindowReady()) {
            fbW = (std::max)(1, ::GetRenderWidth());
            fbH = (std::max)(1, ::GetRenderHeight());
        }
#endif

        s_raylibstate.lastViewport = compute_fit_viewport(fbW, fbH, refW, refH);
    }

    // --- Helper: apply size to Win32 child + raylib/GLFW (after docking) ---
    inline void apply_native_resize(int framebufferW,
        int framebufferH,
        int logicalW,
        int logicalH,
        bool updateNativeWindow,
        bool updateRaylibWindow)
    {
        const int clampedFbW = (std::max)(1, framebufferW);
        const int clampedFbH = (std::max)(1, framebufferH);
        const int clampedLogicalW = (std::max)(1, logicalW);
        const int clampedLogicalH = (std::max)(1, logicalH);

#if defined(_WIN32)
        if (updateNativeWindow && s_raylibstate.hwnd) {
            ::SetWindowPos(s_raylibstate.hwnd, nullptr, 0, 0, clampedFbW, clampedFbH,
                SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        }
#endif

#if !defined(RAYLIB_NO_WINDOW)
        if (updateRaylibWindow && ::IsWindowReady()) {
            // Let raylib manage DPI internally; pass physical-ish intent.
            ::SetWindowSize(clampedLogicalW, clampedLogicalH);
        }
#endif
    }

    inline std::pair<int, int> framebuffer_size() noexcept
    {
#if !defined(RAYLIB_NO_WINDOW)
        if (!::IsWindowReady()) return { 1,1 };
        return { (std::max)(1, GetRenderWidth()), (std::max)(1, GetRenderHeight()) };
#else
        return { 1,1 };
#endif
    }

    inline void update_mouse_scale()
    {
#if !defined(RAYLIB_NO_WINDOW)
        if (!::IsWindowReady()) return;
        const auto [sx, sy] = framebuffer_scale();
        ::SetMouseScale(sx, sy);

        const Vector2 offset = ::GetRenderOffset();
        ::SetMouseOffset(static_cast<int>(std::lround(offset.x)),
            static_cast<int>(std::lround(offset.y)));
#endif
    }

    inline void dispatch_resize(const std::shared_ptr<core::Context>& ctx,
        unsigned int fbWidth,
        unsigned int fbHeight,
        bool updateRaylibWindow,
        bool notifyClient = true,
        bool skipNativeApply = false)
    {
        unsigned int nextFbW = fbWidth;
        unsigned int nextFbH = fbHeight;
        bool nextUpdateWindow = updateRaylibWindow;
        bool nextNotifyClient = notifyClient;
        bool nextSkipNativeApply = skipNativeApply;

        if (s_raylibstate.dispatchingResize)
        {
            s_raylibstate.pendingWidth = nextFbW;
            s_raylibstate.pendingHeight = nextFbH;
            s_raylibstate.pendingUpdateWindow = s_raylibstate.pendingUpdateWindow || nextUpdateWindow;
            s_raylibstate.pendingNotifyClient = s_raylibstate.pendingNotifyClient || nextNotifyClient;
            s_raylibstate.pendingSkipNativeApply = nextSkipNativeApply;
            s_raylibstate.hasPendingResize = true;
            return;
        }

        s_raylibstate.dispatchingResize = true;

        for (;;)
        {
            const unsigned int safeFbW = (std::max)(1u, nextFbW);
            const unsigned int safeFbH = (std::max)(1u, nextFbH);

            s_raylibstate.width = safeFbW;
            s_raylibstate.height = safeFbH;

            if (ctx) {
                ctx->framebufferWidth = static_cast<int>(safeFbW);
                ctx->framebufferHeight = static_cast<int>(safeFbH);
            }

            // Logical window size (for info only)
            unsigned int logicalW = safeFbW, logicalH = safeFbH;
#if !defined(RAYLIB_NO_WINDOW)
            if (::IsWindowReady()) {
                logicalW = (std::max)(1, ::GetScreenWidth());
                logicalH = (std::max)(1, ::GetScreenHeight());
            }
#endif
            s_raylibstate.logicalWidth = logicalW;
            s_raylibstate.logicalHeight = logicalH;

            // Resolve the virtual canvas from framebuffer unless CLI overrides force it.
            const bool widthOverride = core::cli::window_width_overridden
                && core::cli::window_width > 0;
            const bool heightOverride = core::cli::window_height_overridden
                && core::cli::window_height > 0;

            const unsigned int overrideW = widthOverride
                ? static_cast<unsigned int>(core::cli::window_width)
                : 0u;
            const unsigned int overrideH = heightOverride
                ? static_cast<unsigned int>(core::cli::window_height)
                : 0u;

            if (s_raylibstate.designWidth == 0u) {
                s_raylibstate.designWidth = (overrideW > 0u) ? overrideW : safeFbW;
            }
            if (s_raylibstate.designHeight == 0u) {
                s_raylibstate.designHeight = (overrideH > 0u) ? overrideH : safeFbH;
            }

            const unsigned int resolvedVirtualW = (overrideW > 0u)
                ? overrideW
                : (s_raylibstate.designWidth > 0u ? s_raylibstate.designWidth : safeFbW);
            const unsigned int resolvedVirtualH = (overrideH > 0u)
                ? overrideH
                : (s_raylibstate.designHeight > 0u ? s_raylibstate.designHeight : safeFbH);

            if (core::cli::trace_raylib_design_metrics) {
                std::cout << "[Trace][Raylib][Design] fb="
                    << safeFbW << 'x' << safeFbH
                    << " design=" << s_raylibstate.designWidth << 'x' << s_raylibstate.designHeight
                    << " virtual=" << resolvedVirtualW << 'x' << resolvedVirtualH
                    << (widthOverride || heightOverride ? " (CLI override)" : "")
                    << '\n';
            }

            const int refW = static_cast<int>((std::max)(1u, resolvedVirtualW));
            const int refH = static_cast<int>((std::max)(1u, resolvedVirtualH));

            // Mirror VIRTUAL size into ctx for GUI/sprite math and keep framebuffer/logical persisted.
            if (ctx) {
                ctx->virtualWidth = refW;
                ctx->virtualHeight = refH;
                ctx->width = refW;
                ctx->height = refH;
            }

            s_raylibstate.virtualWidth = resolvedVirtualW;
            s_raylibstate.virtualHeight = resolvedVirtualH;

            // Seed the viewport fit immediately so the very next draw call uses
            // the correct logical-to-framebuffer transform instead of a 1×1
            // placeholder from the default-initialised state. This keeps atlas
            // driven widgets from clipping or drifting while the window is
            // still processing the resize notification on the render thread.
            s_raylibstate.lastViewport = compute_fit_viewport(
                static_cast<int>(safeFbW),
                static_cast<int>(safeFbH),
                refW,
                refH);

#if !defined(RAYLIB_NO_WINDOW)
            if (::IsWindowReady()) {
                // Raylib aligns the framebuffer to the desktop until the child
                // window is fully docked. Re-seed the viewport using the
                // actual render dimensions so clipping matches the live
                // backbuffer even before the next resize callback arrives.
                seed_viewport_from_framebuffer(ctx);
            }
#endif

            bool hasNativeParent = false;
#if defined(_WIN32)
            HWND observedParent = nullptr;
            if (s_raylibstate.hwnd) {
                observedParent = ::GetParent(s_raylibstate.hwnd);
                if (observedParent && observedParent != ::GetDesktopWindow()) {
                    hasNativeParent = true;
                }
                else {
                    observedParent = nullptr;
                }
                s_raylibstate.parent = observedParent;
            }
            else if (s_raylibstate.parent) {
                hasNativeParent = true;
            }
#endif

            const bool updateNativeWindow = !nextSkipNativeApply && hasNativeParent;
            const bool updateRaylibWindow = !nextSkipNativeApply && (nextUpdateWindow || hasNativeParent);

            if (!nextSkipNativeApply) {
                const int framebufferW = static_cast<int>(safeFbW);
                const int framebufferH = static_cast<int>(safeFbH);
                const int logicalWInt = static_cast<int>((std::max)(1u, logicalW));
                const int logicalHInt = static_cast<int>((std::max)(1u, logicalH));
                const int raylibTargetW = nextUpdateWindow ? logicalWInt : framebufferW;
                const int raylibTargetH = nextUpdateWindow ? logicalHInt : framebufferH;

                apply_native_resize(framebufferW,
                    framebufferH,
                    raylibTargetW,
                    raylibTargetH,
                    updateNativeWindow,
                    updateRaylibWindow);
            }

            if (nextNotifyClient && s_raylibstate.clientOnResize) {
                try {
                    s_raylibstate.clientOnResize(static_cast<int>(safeFbW),
                        static_cast<int>(safeFbH));
                }
                catch (const std::exception& e) {
                    std::cerr << "[Raylib] onResize client callback threw: " << e.what() << "\n";
                }
                catch (...) {
                    std::cerr << "[Raylib] onResize client callback threw unknown exception.\n";
                }
            }

            if (!s_raylibstate.hasPendingResize) break;

            // Drain pending coalesced resize
            nextFbW = s_raylibstate.pendingWidth;
            nextFbH = s_raylibstate.pendingHeight;
            nextUpdateWindow = s_raylibstate.pendingUpdateWindow;
            nextNotifyClient = s_raylibstate.pendingNotifyClient;
            nextSkipNativeApply = s_raylibstate.pendingSkipNativeApply;

            s_raylibstate.hasPendingResize = false;
            s_raylibstate.pendingUpdateWindow = false;
            s_raylibstate.pendingNotifyClient = false;
            s_raylibstate.pendingSkipNativeApply = false;
        }

        s_raylibstate.dispatchingResize = false;
    }

    inline void sync_framebuffer_size(const std::shared_ptr<core::Context>& ctx,
        bool notifyClient)
    {
#if !defined(RAYLIB_NO_WINDOW)
        if (!::IsWindowReady()) return;

        const int rw = (std::max)(1, ::GetRenderWidth());
        const int rh = (std::max)(1, ::GetRenderHeight());

        dispatch_resize(ctx, (unsigned)rw, (unsigned)rh,
            /*updateRaylibWindow=*/false,
            notifyClient,
            /*skipNativeApply=*/true);
#endif
    }

    // ──────────────────────────────────────────────
    // Initialize Raylib window and context
    // ──────────────────────────────────────────────
    inline bool raylib_initialize(std::shared_ptr<core::Context> ctx,
        NativeWindowHandle parentWnd = nullptr,
        unsigned int w = 800,
        unsigned int h = 600,
        std::function<void(int, int)> onResize = nullptr,
        std::string windowTitle = {})
    {
        const unsigned int clampedWidth = (std::max)(1u, w);
        const unsigned int clampedHeight = (std::max)(1u, h);

        s_raylibstate.clientOnResize = std::move(onResize);
        s_raylibstate.parent = parentWnd;
        s_raylibstate.dispatchingResize = false;
        s_raylibstate.hasPendingResize = false;
        s_raylibstate.pendingUpdateWindow = false;
        s_raylibstate.pendingNotifyClient = false;
        s_raylibstate.pendingSkipNativeApply = false;
        s_raylibstate.pendingWidth = 0;
        s_raylibstate.pendingHeight = 0;

        std::weak_ptr<core::Context> ctxWeak = ctx;
        s_raylibstate.onResize = [ctxWeak](int fbW, int fbH)
            {
                const int safeW = (std::max)(1, fbW);
                const int safeH = (std::max)(1, fbH);
                if (auto locked = ctxWeak.lock()) {
                    dispatch_resize(locked, (unsigned)safeW, (unsigned)safeH,
                        /*updateRaylibWindow=*/false,
                        /*notifyClient=*/false);
                }
            };
        if (ctx) ctx->onResize = s_raylibstate.onResize;

        const bool widthOverride = core::cli::window_width_overridden
            && core::cli::window_width > 0;
        const bool heightOverride = core::cli::window_height_overridden
            && core::cli::window_height > 0;

        const unsigned int requestedDesignW = widthOverride
            ? static_cast<unsigned int>(core::cli::window_width)
            : clampedWidth;
        const unsigned int requestedDesignH = heightOverride
            ? static_cast<unsigned int>(core::cli::window_height)
            : clampedHeight;

        if (s_raylibstate.designWidth == 0u) {
            s_raylibstate.designWidth = requestedDesignW;
        }
        if (s_raylibstate.designHeight == 0u) {
            s_raylibstate.designHeight = requestedDesignH;
        }

        // Seed sizes; treat given w/h as framebuffer pixels to start
        dispatch_resize(ctx, clampedWidth, clampedHeight, /*updateRaylibWindow=*/false, /*notifyClient=*/false);

        static bool initialized = false;
        if (initialized) return true;
        initialized = true;

        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
        if (windowTitle.empty()) windowTitle = "Raylib Window";

        InitWindow((int)s_raylibstate.width, (int)s_raylibstate.height, windowTitle.c_str());
        SetWindowTitle(windowTitle.c_str());

        seed_viewport_from_framebuffer(ctx);

        // First sync to actual framebuffer + logical sizes
        sync_framebuffer_size(ctx, /*notifyClient=*/false);

#if defined(_WIN32)
        const auto previousDC = detail::current_dc();
        const auto previousContext = detail::current_context();

        s_raylibstate.hwnd = static_cast<HWND>(GetWindowHandle());
        s_raylibstate.ownsDC = false;

        s_raylibstate.hdc = previousDC;
        if (!s_raylibstate.hdc && s_raylibstate.hwnd) {
            s_raylibstate.hdc = GetDC(s_raylibstate.hwnd);
            s_raylibstate.ownsDC = (s_raylibstate.hdc != nullptr);
        }

        s_raylibstate.glContext = previousContext;
        if (!s_raylibstate.glContext) {
            s_raylibstate.glContext = detail::current_context();
        }

        if (!s_raylibstate.hdc || !s_raylibstate.glContext) {
            std::cerr << "[Raylib] Failed to acquire Win32 GL handles after InitWindow" << std::endl;
            if (s_raylibstate.ownsDC && s_raylibstate.hdc && s_raylibstate.hwnd) {
                ::ReleaseDC(s_raylibstate.hwnd, s_raylibstate.hdc);
            }
            s_raylibstate.hdc = nullptr;
            s_raylibstate.glContext = nullptr;
            s_raylibstate.ownsDC = false;
            initialized = false;
            return false;
        }

        if (!detail::make_current(s_raylibstate.hdc, s_raylibstate.glContext)) {
            std::cerr << "[Raylib] Failed to make Raylib context current during initialization" << std::endl;
            if (s_raylibstate.ownsDC && s_raylibstate.hdc && s_raylibstate.hwnd) {
                ::ReleaseDC(s_raylibstate.hwnd, s_raylibstate.hdc);
            }
            s_raylibstate.hdc = nullptr;
            s_raylibstate.glContext = nullptr;
            s_raylibstate.ownsDC = false;
            initialized = false;
            return false;
        }

        if (!detail::contexts_match(previousDC, previousContext,
            s_raylibstate.hdc, s_raylibstate.glContext)) {
            if (previousDC && previousContext) {
                detail::make_current(previousDC, previousContext);
            }
            else {
                detail::clear_current();
            }
        }
#else
        s_raylibstate.hwnd = GetWindowHandle();
        s_raylibstate.hdc = nullptr;
        s_raylibstate.glContext = nullptr;
        s_raylibstate.ownsDC = false;
#endif

        if (ctx) {
            ctx->hwnd = s_raylibstate.hwnd;
            ctx->hdc = s_raylibstate.hdc;
            ctx->hglrc = s_raylibstate.glContext;
            // ctx->width/height already set to virtual in dispatch_resize
            ctx->get_mouse_position = [](int& outX, int& outY) noexcept {
#if !defined(RAYLIB_NO_WINDOW)
                if (!::IsWindowReady()) {
                    outX = -1;
                    outY = -1;
                    return;
                }

                const GuiFitViewport fit = s_raylibstate.lastViewport;
                const Vector2 position = ::GetMousePosition();

                const bool inside =
                    (position.x >= 0.0f && position.y >= 0.0f &&
                        position.x < static_cast<float>(fit.refW) &&
                        position.y < static_cast<float>(fit.refH));

                if (inside) {
                    outX = static_cast<int>(std::lround(position.x));
                    outY = static_cast<int>(std::lround(position.y));
                }
                else {
                    outX = -1;
                    outY = -1;
                }

#ifndef NDEBUG
                const unsigned int expectedW = s_raylibstate.virtualWidth;
                const unsigned int expectedH = s_raylibstate.virtualHeight;
                assert(static_cast<int>(expectedW) == fit.refW);
                assert(static_cast<int>(expectedH) == fit.refH);
#endif
#else
                outX = -1;
                outY = -1;
#endif
            };
        }

        // If docking into a parent, reparent + single hard resize pass
        if (s_raylibstate.parent) {
#if defined(_WIN32)
            ::SetParent(s_raylibstate.hwnd, s_raylibstate.parent);
            ::ShowWindow(s_raylibstate.hwnd, SW_SHOW);

            LONG_PTR style = ::GetWindowLongPtr(s_raylibstate.hwnd, GWL_STYLE);
            style &= ~WS_OVERLAPPEDWINDOW;
            style |= (WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
            ::SetWindowLongPtr(s_raylibstate.hwnd, GWL_STYLE, style);

            LONG_PTR ex = ::GetWindowLongPtr(s_raylibstate.hwnd, GWL_EXSTYLE);
            ex &= ~WS_EX_APPWINDOW;
            ::SetWindowLongPtr(s_raylibstate.hwnd, GWL_EXSTYLE, ex);

            almondnamespace::core::MakeDockable(s_raylibstate.hwnd, s_raylibstate.parent);

            if (!windowTitle.empty()) {
                const std::wstring wideTitle(windowTitle.begin(), windowTitle.end());
                ::SetWindowTextW(s_raylibstate.hwnd, wideTitle.c_str());
            }

            RECT client{};
            ::GetClientRect(s_raylibstate.parent, &client);
            const int pw = static_cast<int>((std::max)(static_cast<LONG>(1), client.right - client.left));
            const int ph = static_cast<int>((std::max)(static_cast<LONG>(1), client.bottom - client.top));

            apply_native_resize(pw, ph, pw, ph, /*native*/true, /*raylib*/true);

            // Sync from real framebuffer
            sync_framebuffer_size(ctx, /*notifyClient=*/true);
            seed_viewport_from_framebuffer(ctx);
#endif
        }

        s_raylibstate.cleanupIssued = false;
        s_raylibstate.running = true;

        // Hook atlas uploads
        atlasmanager::register_backend_uploader(core::ContextType::RayLib,
            [](const TextureAtlas& atlas) {
                raylibtextures::ensure_uploaded(atlas);
            });

        return true;
    }

    inline void Raylib_CloseWindow() { /* no-op placeholder */ }

    // ──────────────────────────────────────────────
    // Per-frame event processing
    // ──────────────────────────────────────────────
    inline bool raylib_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue)
    {
        if (!s_raylibstate.running || WindowShouldClose()) {
            s_raylibstate.running = false;
            return true;
        }

        // Observe current framebuffer size
        const unsigned int prevFbW = s_raylibstate.width;
        const unsigned int prevFbH = s_raylibstate.height;

        const int rw = (std::max)(1, GetRenderWidth());
        const int rh = (std::max)(1, GetRenderHeight());

#if !defined(RAYLIB_NO_WINDOW)
        const bool windowReady = ::IsWindowReady();
        const bool raylibReportedResize = windowReady && ::IsWindowResized();
        const int sw = windowReady ? (std::max)(1, ::GetScreenWidth())
            : static_cast<int>(s_raylibstate.logicalWidth);
        const int sh = windowReady ? (std::max)(1, ::GetScreenHeight())
            : static_cast<int>(s_raylibstate.logicalHeight);
#else
        const bool raylibReportedResize = false;
        const int sw = static_cast<int>(s_raylibstate.logicalWidth);
        const int sh = static_cast<int>(s_raylibstate.logicalHeight);
#endif

        const bool framebufferChanged = (unsigned)rw != prevFbW || (unsigned)rh != prevFbH;
        const bool logicalChanged = (unsigned)(std::max)(1, sw) != s_raylibstate.logicalWidth
            || (unsigned)(std::max)(1, sh) != s_raylibstate.logicalHeight;

        if (raylibReportedResize || framebufferChanged || logicalChanged) {
            dispatch_resize(ctx, (unsigned)rw, (unsigned)rh,
                /*updateRaylibWindow=*/raylibReportedResize,
                /*notifyClient=*/true,
                /*skipNativeApply=*/!raylibReportedResize);
        }

#if defined(_WIN32)
        static bool reportedMissingContext = false;
        static bool reportedMakeCurrentFailure = false;
        if (!detail::refresh_wgl_handles()) {
            if (!reportedMakeCurrentFailure) {
                reportedMakeCurrentFailure = true;
                std::cerr << "[Raylib] Failed to refresh/make Raylib GL context current; skipping frame\n";
            }
            return true;
        }
        reportedMakeCurrentFailure = false;

        if (s_raylibstate.hdc && s_raylibstate.glContext) {
            reportedMissingContext = false;
        }
        else {
            if (!reportedMissingContext) {
                reportedMissingContext = true;
                std::cerr << "[Raylib] Render context became unavailable (hdc="
                    << s_raylibstate.hdc << ", hglrc=" << s_raylibstate.glContext << ")\n";
            }
#ifndef NDEBUG
            assert(false && "Raylib GL context lost");
#endif
        }
#endif

        atlasmanager::process_pending_uploads(core::ContextType::RayLib);

        // ----- VIRTUAL FIT VIEWPORT + MOUSE MAPPING -----
        const int fbW = (std::max)(1, GetRenderWidth());
        const int fbH = (std::max)(1, GetRenderHeight());

        const unsigned int designFallbackW = (s_raylibstate.designWidth > 0u)
            ? s_raylibstate.designWidth
            : static_cast<unsigned int>((std::max)(1, fbW));
        const unsigned int designFallbackH = (s_raylibstate.designHeight > 0u)
            ? s_raylibstate.designHeight
            : static_cast<unsigned int>((std::max)(1, fbH));

        const unsigned int ctxVirtualW = (ctx && ctx->virtualWidth > 0)
            ? static_cast<unsigned int>(ctx->virtualWidth)
            : 0u;
        const unsigned int ctxVirtualH = (ctx && ctx->virtualHeight > 0)
            ? static_cast<unsigned int>(ctx->virtualHeight)
            : 0u;

        const unsigned int stateVirtualW = (s_raylibstate.virtualWidth > 0u)
            ? s_raylibstate.virtualWidth
            : 0u;
        const unsigned int stateVirtualH = (s_raylibstate.virtualHeight > 0u)
            ? s_raylibstate.virtualHeight
            : 0u;

        const unsigned int resolvedRefW = (ctxVirtualW > 0u)
            ? ctxVirtualW
            : (stateVirtualW > 0u ? stateVirtualW : designFallbackW);
        const unsigned int resolvedRefH = (ctxVirtualH > 0u)
            ? ctxVirtualH
            : (stateVirtualH > 0u ? stateVirtualH : designFallbackH);

        const int refW = static_cast<int>((std::max)(1u, resolvedRefW));
        const int refH = static_cast<int>((std::max)(1u, resolvedRefH));

        const GuiFitViewport fit = compute_fit_viewport(fbW, fbH, refW, refH);

        // Persist resolved dimensions on the shared context
        if (ctx) {
            ctx->framebufferWidth = fbW;
            ctx->framebufferHeight = fbH;
            ctx->virtualWidth = refW;
            ctx->virtualHeight = refH;
            ctx->width = fit.refW;
            ctx->height = fit.refH;
        }

        // Viewport for rendering (letterbox/pillarbox)
        s_raylibstate.lastViewport = fit;
        s_raylibstate.virtualWidth = static_cast<unsigned int>((std::max)(1, fit.refW));
        s_raylibstate.virtualHeight = static_cast<unsigned int>((std::max)(1, fit.refH));

#ifndef NDEBUG
        if (ctx) {
            const bool viewportMismatch =
                (ctx->width != fit.refW) || (ctx->height != fit.refH);
            if (viewportMismatch) {
                std::cerr << "[Raylib] Viewport mismatch: Context="
                    << ctx->width << 'x' << ctx->height
                    << " Fit=" << fit.refW << 'x' << fit.refH << "\n";
            }
            assert(!viewportMismatch);
        }
#endif

        // Mouse scaling/offset must follow the viewport so GUI hit-testing aligns
#if !defined(RAYLIB_NO_WINDOW)
        if (::IsWindowReady()) {
            const Vector2 renderOffset = ::GetRenderOffset();
            const int baseOffsetX = static_cast<int>(std::floor(renderOffset.x)) + fit.vpX;
            const int baseOffsetY = static_cast<int>(std::floor(renderOffset.y)) + fit.vpY;
            const float invScale = (fit.scale > 0.0f) ? (1.0f / fit.scale) : 1.0f;
            ::SetMouseOffset(-baseOffsetX, -baseOffsetY);
            ::SetMouseScale(invScale, invScale);
        }
#endif
        // -----------------------------------------------

        // Animated bg (helps verify viewport)
        static auto* bgTimer = almondnamespace::time::getTimer("menu", "bg_color");
        if (!bgTimer) bgTimer = &almondnamespace::time::createNamedTimer("menu", "bg_color");
        const double t = almondnamespace::time::elapsed(*bgTimer);

        const unsigned char r = static_cast<unsigned char>((0.5 + 0.5 * std::sin(t * 1.0)) * 255);
        const unsigned char g = static_cast<unsigned char>((0.5 + 0.5 * std::sin(t * 0.7 + 2.0)) * 255);
        const unsigned char b = static_cast<unsigned char>((0.5 + 0.5 * std::sin(t * 1.3 + 4.0)) * 255);

        BeginDrawing();
        ClearBackground(Color{ r, g, b, 255 });

        int scissorX = fit.vpX;
        int scissorY = fit.vpY;
#if !defined(RAYLIB_NO_WINDOW)
        if (IsWindowReady()) {
            const Vector2 renderOffset = GetRenderOffset();
            scissorX += static_cast<int>(std::floor(renderOffset.x));
            scissorY += static_cast<int>(std::floor(renderOffset.y));
        }
#endif

        BeginScissorMode(scissorX, scissorY, fit.vpW, fit.vpH);

        // NOTE: Your draw_sprite path uses ctx->width/height → now virtual.
        // That guarantees consistent atlas button sizing/placement.
        queue.drain();

        EndScissorMode();

        EndDrawing();

        return true; // continue running
    }

    inline void raylib_clear()
    {
        BeginDrawing();
        ClearBackground(DARKPURPLE);
        EndDrawing();
    }

    inline void raylib_present() { /* EndDrawing() presents already */ }

    // ──────────────────────────────────────────────
    // Cleanup and shutdown
    // ──────────────────────────────────────────────
    inline void raylib_cleanup(std::shared_ptr<almondnamespace::core::Context>& ctx)
    {
        if (s_raylibstate.cleanupIssued) {
            return;
        }
        s_raylibstate.cleanupIssued = true;

#if defined(_WIN32)
        if (s_raylibstate.hdc && s_raylibstate.glContext) {
            detail::clear_current();
        }
#endif

#if !defined(RAYLIB_NO_WINDOW)
        if (IsWindowReady()) {
            CloseWindow();
        }
#endif

#if defined(_WIN32)
        if (s_raylibstate.hwnd && ::IsWindow(s_raylibstate.hwnd)) {
            if (s_raylibstate.ownsDC && s_raylibstate.hdc) {
                ::ReleaseDC(s_raylibstate.hwnd, s_raylibstate.hdc);
            }
            ::DestroyWindow(s_raylibstate.hwnd);
        }
        s_raylibstate.hwnd = nullptr;
        s_raylibstate.hdc = nullptr;
        s_raylibstate.glContext = nullptr;
        s_raylibstate.ownsDC = false;
#endif

        s_raylibstate.running = false;
        s_raylibstate.width = 1;
        s_raylibstate.height = 1;
        s_raylibstate.virtualWidth = 1;
        s_raylibstate.virtualHeight = 1;

        if (ctx) {
            ctx->hwnd = nullptr;
            ctx->hdc = nullptr;
            ctx->hglrc = nullptr;
        }
    }

    // ──────────────────────────────────────────────
    // Helpers
    // ──────────────────────────────────────────────
    inline int  raylib_get_width()  noexcept { return static_cast<int>(s_raylibstate.virtualWidth); }
    inline int  raylib_get_height() noexcept { return static_cast<int>(s_raylibstate.virtualHeight); }
    inline void raylib_set_window_title(const std::string& title) { SetWindowTitle(title.c_str()); }
    inline bool RaylibIsRunning(std::shared_ptr<core::Context>) { return s_raylibstate.running; }

} // namespace almondnamespace::raylibcontext

#endif // ALMOND_USING_RAYLIB
