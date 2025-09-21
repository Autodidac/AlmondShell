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

#include "aengineconfig.hpp"

#if defined(ALMOND_USING_RAYLIB)


#include "acontext.hpp"
#include "awindowdata.hpp"

#include "acontextwindow.hpp"
#include "acontextstatemanager.hpp"
#include "arobusttime.hpp"
#include "aimageloader.hpp"
#include "araylibtextures.hpp" // AtlasGPU, gpu_atlases, ensure_uploaded
#include "araylibstate.hpp" // brings in the actual inline s_state
#include "araylibrenderer.hpp" // RendererContext, RenderMode
#include "acommandline.hpp" // cli::window_width, cli::window_height
#include "araylibcontextinput.hpp" // poll_input()

//#include "araylibcontext_win32.hpp"

#include <stdexcept>
#include <iostream>
//#include <windows.h>
// Raylib includes 
#include <raylib.h> // Ensure this is included after platform-specific headers
namespace almondnamespace::raylibcontext
{
    // Usually at the start of your program, before window creation:
    static almondnamespace::contextwindow::WindowData* g_raylibwindowContext;

   // HWND hwnd;
    struct RaylibState {
        HWND parent{};
        HWND hwnd{};
        HDC  hdc{};       // Store device context
        HGLRC glContext{}; // Store GL context created by Raylib
        bool running{ false };
        unsigned int width{ 800 };
        unsigned int height{ 600 };
        std::function<void(int, int)> onResize;
    };
    inline RaylibState raylibcontext;
    
    // ──────────────────────────────────────────────
    // Initialize Raylib window and context
    // ──────────────────────────────────────────────
    inline bool raylib_initialize(std::shared_ptr<core::Context> ctx, HWND parentWnd = nullptr, unsigned int w = 800, unsigned int h = 600, std::function<void(int, int)> onResize = nullptr)
    {
       // if (ctx) ctx->windowData = core::WindowData::get_global_instance();

        raylibcontext.onResize = std::move(onResize);
        raylibcontext.width = w;
        raylibcontext.height = h;
        raylibcontext.parent = parentWnd;

        InitWindow(raylibcontext.width, raylibcontext.height, "Raylib Window");
        // SetTargetFPS(60);  // Cap FPS to something sane

        raylibcontext.hwnd = (HWND)GetWindowHandle();
        raylibcontext.hdc = GetDC(raylibcontext.hwnd);
        raylibcontext.glContext = wglGetCurrentContext();  // Get Raylib's GL context _immediately_ after InitWindow

        std::cout << "[Raylib] Context: " << raylibcontext.glContext << "\n";

        if (raylibcontext.parent) {
            RECT parentRect;
            GetWindowRect(raylibcontext.parent, &parentRect);
            SetParent(raylibcontext.hwnd, raylibcontext.parent);
            ShowWindow(raylibcontext.hwnd, SW_SHOW);

            LONG_PTR style = GetWindowLongPtr(raylibcontext.hwnd, GWL_STYLE);
            style &= ~WS_OVERLAPPEDWINDOW;
            style |= WS_CHILD | WS_VISIBLE;
            SetWindowLongPtr(raylibcontext.hwnd, GWL_STYLE, style);

            SetWindowPos(raylibcontext.hwnd, nullptr, raylibcontext.width, 0,
                raylibcontext.width,
                raylibcontext.height,
                SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

            RECT rc;
            GetClientRect(raylibcontext.parent, &rc);
            PostMessage(raylibcontext.parent, WM_SIZE, 0, MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));
        }

        raylibcontext.running = true;
        return true;

//        //InitRaylibWindow();
//        // 1. Ensure a context exists
//        //if (!almondnamespace::ContextManager::hasContext()) {
//        //    almondnamespace::ContextManager::setContext(std::make_shared<almondnamespace::state::ContextState>());
//        //}
//
//        // Optional: you can sync these to your context window module
//       // s_state.screenWidth = s_state.window.get_width()
//       // s_state.screenHeight = s_state.window.get_height()
//
//        InitWindow(s_raylibstate.screenWidth, s_raylibstate.screenHeight, "AlmondEngine - Raylib");
//#if defined(_WIN32)
////        s_raylibstate.hwnd = (HWND)GetWindowHandle();
//        //HWND raylibHwnd = (HWND)GetWindowHandle();
//       // s_raylibstate.hwnd = raylibHwnd; // Store for your engine
//	//	DockRaylibWindow(raylibHwnd);
//
//        // Allocate and configure your context
//        auto* winCtx = new almondnamespace::contextwindow::WindowData();
//        //winCtx->setWindowHandle((HWND)GetWindowHandle()); // Raylib's HWND
//        almondnamespace::contextwindow::WindowData::set_global_instance(winCtx);
//        HWND hwndParent = winCtx->getWindowHandle();
//       // winCtx->setParentHandle(hwndParent);
//        winCtx->setParentHandle(hwndParent);
//       // almondnamespace::contextwindow::WindowData::setParentHandle(hwndParent);
//        winCtx->setChildHandle((HWND)GetWindowHandle()); // Set this if you want to dock Raylib window into a parent window
//        HWND hwndChild = winCtx ->getChildHandle();
//        
//        //core::create_window();
//        // 4. Optionally adjust window style for child behavior
//        //POINT cursor;
//        //GetCursorPos(&cursor);
//
//        //RECT parentRect;
//        //GetWindowRect(hwndParent, &parentRect);
//        ////if (PtInRect(&parentRect, cursor)) {
//        LONG_PTR style = GetWindowLongPtr(hwndChild, GWL_STYLE);
//        style &= ~WS_OVERLAPPED;
//        style |= WS_CHILD | WS_VISIBLE;
//        SetWindowLongPtr(hwndChild, GWL_STYLE, style);
//        SetParent(hwndChild, hwndParent);
//        SetWindowPos(hwndChild, nullptr, 0, 0, core::cli::window_width, core::cli::window_height, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
//
//        RECT rc;
//        GetClientRect(hwndParent, &rc);
//        PostMessage(hwndParent, WM_SIZE, 0, MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));
//
//        //almondnamespace::core::create_window(hi, rc.right - rc.left, rc.bottom - rc.top, L"AlmondChild", L"Child", hwndParent);
//
//        //hwnd = (HWND)GetWindowHandle();
//        //almondnamespace::contextwindow::WindowContext::set_global_instance(g_raylibwindowContext);
//        // Dock Raylib window into parent if requested (Win32 only, not officially supported by Raylib)
//        //if (parentHwnd && s_raylibstate.hwnd) {
//           // SetParent(s_raylibstate.hwnd, parentHwnd);
//            // Optionally adjust window style here if needed
//       // }
//       // s_raylibstate.hwnd = contextwindow::WindowContext::getWindowHandle();
//#endif
//
//
//        // You can enable VSync or multi-sampling etc.
//        //SetTargetFPS(60);
//        return true;
    }

    inline void Raylib_CloseWindow() { 
        //::CloseWindow(s_raylibstate.hwnd);
//        if (hwnd)
 //           PostMessage(hwnd, WM_CLOSE, 0, 0); // ask window to close gracefully
    }
    // ──────────────────────────────────────────────
    // Per-frame event processing
    // ──────────────────────────────────────────────
    inline bool raylib_process(core::Context& ctx, core::CommandQueue& queue) {
        if (!raylibcontext.running || WindowShouldClose()) {
            raylibcontext.running = false;
            return true;
        }

        if (!wglMakeCurrent(raylibcontext.hdc, raylibcontext.glContext)) {
            raylibcontext.running = false;
            std::cerr << "[Raylib] Failed to make Raylib GL context current\n";
            return true;
        }
        //std::cout << "Running! " << '\n';
        // inside your raylib process function
        static auto* bgTimer = almondnamespace::time::getTimer("menu", "bg_color");
        if (!bgTimer)
            bgTimer = &almondnamespace::time::createNamedTimer("menu", "bg_color");

        double t = almondnamespace::time::elapsed(*bgTimer); // seconds

        // Map sin output [−1,1] to [0,255] for Raylib Color
        unsigned char r = static_cast<unsigned char>((0.5 + 0.5 * std::sin(t * 1.0)) * 255);
        unsigned char g = static_cast<unsigned char>((0.5 + 0.5 * std::sin(t * 0.7 + 2.0)) * 255);
        unsigned char b = static_cast<unsigned char>((0.5 + 0.5 * std::sin(t * 1.3 + 4.0)) * 255);


        BeginDrawing();
       // ClearBackground(RED);
        ClearBackground(Color{ r, g, b, 255 });
        //DRAWTEXT(TextFormat("Raylib Rendering OK"), 200, 160, 40, BLUE);

        EndDrawing();

        // Optional: unbind context if you want to be neat
        // wglMakeCurrent(nullptr, nullptr);
        //almondnamespace::raylibcontext::poll_input();
        //s_raylibstate.shouldClose = WindowShouldClose();
        //return !s_raylibstate.shouldClose;
		return true; // Continue running
    }

    // ──────────────────────────────────────────────
    // Clear the screen — Raylib style
    // ──────────────────────────────────────────────
    inline void raylib_clear()
    {
        BeginDrawing();
        ClearBackground(DARKPURPLE);
    }

    // ──────────────────────────────────────────────
    // Present your frame — Raylib style
    // ──────────────────────────────────────────────
    inline void raylib_present()
    {
        EndDrawing();
    }

    // ──────────────────────────────────────────────
    // Cleanup and shutdown
    // ──────────────────────────────────────────────
    inline void raylib_cleanup(std::shared_ptr<almondnamespace::core::Context>& ctx) {
        if (!raylibcontext.running) return;
        CloseWindow(raylibcontext.hwnd);  // Raylib manages its window internally
        raylibcontext.running = false;
       // Raylib_CloseWindow();
    }

    // ──────────────────────────────────────────────
    // Extra helpers if you like consistency
    // ──────────────────────────────────────────────
    inline int raylib_get_width()  noexcept { return GetScreenWidth(); }
    inline int raylib_get_height() noexcept { return GetScreenHeight(); }
    inline void raylib_set_window_title(const std::string& title)
    {
        SetWindowTitle(title.c_str());
    }

    inline bool RaylibIsRunning(std::shared_ptr<core::Context> ctx) {
        return raylibcontext.running;
    }
} // namespace almondnamespace::raylibcontext

#endif // ALMOND_USING_RAYLIB
