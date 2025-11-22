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
 // asdlcontext.hpp
#pragma once

#include "aengineconfig.hpp"

#if defined(ALMOND_USING_SDL)

// Add the following definition at the top of the file, after the includes, to define SDL_WINDOW_FULLSCREEN_DESKTOP if it is not already defined.
#ifndef SDL_WINDOW_FULLSCREEN_DESKTOP
#define SDL_WINDOW_FULLSCREEN_DESKTOP 1  // SDL3 equivalent for fullscreen desktop mode
#endif

#include "aplatform.hpp"

#define SDL_MAIN_HANDLED // Allow SDL to handle main function, useful for SDL applications

#include "acontext.hpp"
#include "acontextwindow.hpp"
#include "asdlcontextrenderer.hpp"
#include "asdltextures.hpp"
#include "aatlasmanager.hpp"
#include "asdlstate.hpp"

#if defined(_WIN32)
namespace almondnamespace::core { void MakeDockable(HWND hwnd, HWND parent); }
#endif

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <string>

namespace almondnamespace::sdlcontext
{
    struct SDLState {
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;

        HWND parent = nullptr;
        HWND hwnd = nullptr;
        bool running = false;
        int width = 400;
        int height = 300;
        int framebufferWidth = 400;
        int framebufferHeight = 300;
        int virtualWidth = 400;
        int virtualHeight = 300;
        std::function<void(int, int)> onResize;
    };

    inline SDLState sdlcontext;

    inline void refresh_dimensions(const std::shared_ptr<core::Context>& ctx) noexcept
    {
        int logicalW = (std::max)(1, sdlcontext.width);
        int logicalH = (std::max)(1, sdlcontext.height);

        if (sdlcontext.window)
        {
            int windowW = 0;
            int windowH = 0;
            SDL_GetWindowSize(sdlcontext.window, &windowW, &windowH);
            if (windowW > 0 && windowH > 0)
            {
                logicalW = windowW;
                logicalH = windowH;
            }
        }

        sdlcontext.width = logicalW;
        sdlcontext.height = logicalH;
        sdlcontext.virtualWidth = logicalW;
        sdlcontext.virtualHeight = logicalH;

        int fbW = logicalW;
        int fbH = logicalH;
        if (sdlcontext.renderer)
        {
            int renderW = 0;
            int renderH = 0;
            if (SDL_GetCurrentRenderOutputSize(sdlcontext.renderer, &renderW, &renderH) == 0
                && renderW > 0 && renderH > 0)
            {
                fbW = renderW;
                fbH = renderH;
            }
        }

        sdlcontext.framebufferWidth = (std::max)(1, fbW);
        sdlcontext.framebufferHeight = (std::max)(1, fbH);

        if (ctx)
        {
            ctx->width = sdlcontext.width;
            ctx->height = sdlcontext.height;
            ctx->virtualWidth = sdlcontext.virtualWidth;
            ctx->virtualHeight = sdlcontext.virtualHeight;
            ctx->framebufferWidth = sdlcontext.framebufferWidth;
            ctx->framebufferHeight = sdlcontext.framebufferHeight;
        }
    }

    inline bool sdl_initialize(std::shared_ptr<core::Context> ctx,
        HWND parentWnd = nullptr,
        int w = 400,
        int h = 300,
        std::function<void(int, int)> onResize = nullptr,
        std::string windowTitle = {})
    {
        const int clampedWidth = (std::max)(1, w);
        const int clampedHeight = (std::max)(1, h);

        sdlcontext.width = clampedWidth;
        sdlcontext.height = clampedHeight;
        sdlcontext.virtualWidth = clampedWidth;
        sdlcontext.virtualHeight = clampedHeight;
        sdlcontext.framebufferWidth = clampedWidth;
        sdlcontext.framebufferHeight = clampedHeight;
        sdlcontext.parent = parentWnd;

        refresh_dimensions(ctx);

        std::weak_ptr<core::Context> weakCtx = ctx;
        auto userResize = std::move(onResize);
        sdlcontext.onResize = [weakCtx, userResize = std::move(userResize)](int width, int height) mutable {
            sdlcontext.width = (std::max)(1, width);
            sdlcontext.height = (std::max)(1, height);
            sdlcontext.virtualWidth = sdlcontext.width;
            sdlcontext.virtualHeight = sdlcontext.height;

            if (sdlcontext.window) {
                SDL_SetWindowSize(sdlcontext.window, sdlcontext.width, sdlcontext.height);
            }

            auto locked = weakCtx.lock();
            refresh_dimensions(locked);

            if (userResize) {
                userResize(sdlcontext.framebufferWidth, sdlcontext.framebufferHeight);
            }
        };

        if (ctx) {
            ctx->onResize = sdlcontext.onResize;
        }

        // SDL3 returns 0 on success, negative on failure.
        // Force the type with cast so MSVC can’t mis-deduce
        if (static_cast<int>(SDL_Init(SDL_INIT_VIDEO)) < 0) {
            SDL_Log("SDL_Init failed: %s", SDL_GetError());
            return false;
        }

        if (windowTitle.empty()) {
            windowTitle = "SDL3 Window";
        }

        // Create properties object
        SDL_PropertiesID props = SDL_CreateProperties();
        if (!props) {
            std::cerr << "[SDL] SDL_CreateProperties failed: " << SDL_GetError() << "\n";
            SDL_Quit();
            return false;
        }

        // Set window properties
        SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, windowTitle.c_str());
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, sdlcontext.width);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, sdlcontext.height);
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);

        // Create window with properties
        sdlcontext.window = SDL_CreateWindowWithProperties(props);
        SDL_DestroyProperties(props);

        // SDL_Window* parentWindow = SDL_CreateWindow("Parent Window", 800, 600, 0);

        if (!sdlcontext.window) {
            std::cerr << "[SDL] SDL_CreateWindowWithProperties failed: " << SDL_GetError() << "\n";
            SDL_Quit();
            return false;
        }

        // Retrieve native window handle from SDL on Windows platforms
#if defined(_WIN32)
        SDL_PropertiesID windowProps = SDL_GetWindowProperties(sdlcontext.window);
        if (!windowProps) {
            std::cerr << "[SDL] SDL_GetWindowProperties failed: " << SDL_GetError() << "\n";
            SDL_DestroyWindow(sdlcontext.window);
            SDL_Quit();
            return false;
        }

        HWND hwnd = (HWND)SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        sdlcontext.hwnd = hwnd;
        if (!sdlcontext.hwnd) {
            std::cerr << "[SDL] Failed to retrieve HWND\n";
            SDL_DestroyWindow(sdlcontext.window);
            SDL_Quit();
            return false;
        }

        if (ctx) {
            ctx->hwnd = sdlcontext.hwnd;
        }
#else
        (void)parentWnd;
        if (ctx) {
            ctx->hwnd = nullptr;
        }
#endif

        SDL_SetWindowTitle(sdlcontext.window, windowTitle.c_str());

        // Create renderer
        sdlcontext.renderer = SDL_CreateRenderer(sdlcontext.window, nullptr);
        if (!sdlcontext.renderer) {
            std::cerr << "[SDL] SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
            SDL_DestroyWindow(sdlcontext.window);
            SDL_Quit();
            return false;
        }

        init_renderer(sdlcontext.renderer);
        sdltextures::sdl_renderer = sdlcontext.renderer;

        refresh_dimensions(ctx);

#if defined(_WIN32)
        if (sdlcontext.parent) {
            std::cout << "[SDL] Setting parent window: " << sdlcontext.parent << "\n";
            SetParent(sdlcontext.hwnd, sdlcontext.parent);

            LONG_PTR style = GetWindowLongPtr(sdlcontext.hwnd, GWL_STYLE);
            style &= ~WS_OVERLAPPEDWINDOW;
            style |= WS_CHILD | WS_VISIBLE;
            SetWindowLongPtr(sdlcontext.hwnd, GWL_STYLE, style);

#if defined(_WIN32)
            almondnamespace::core::MakeDockable(sdlcontext.hwnd, sdlcontext.parent);
#endif

            if (!windowTitle.empty()) {
                const std::wstring wideTitle(windowTitle.begin(), windowTitle.end());
                SetWindowTextW(sdlcontext.hwnd, wideTitle.c_str());
            }

            RECT client{};
            GetClientRect(sdlcontext.parent, &client);
            const int width = static_cast<int>((std::max)(static_cast<LONG>(1), client.right - client.left));
            const int height = static_cast<int>((std::max)(static_cast<LONG>(1), client.bottom - client.top));

            sdlcontext.width = width;
            sdlcontext.height = height;

            SetWindowPos(sdlcontext.hwnd, nullptr, 0, 0,
                width, height,
                SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

            if (sdlcontext.onResize) {
                sdlcontext.onResize(width, height);
            }

            PostMessage(sdlcontext.parent, WM_SIZE, 0, MAKELPARAM(width, height));
        }
#endif
        // SDL_SetRenderDrawColor(ctx.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_ShowWindow(sdlcontext.window);
        sdlcontext.running = true;

        atlasmanager::register_backend_uploader(core::ContextType::SDL,
            [](const TextureAtlas& atlas) {
                sdltextures::ensure_uploaded(atlas);
            });


        //SDL_Window* sdlParent = SDL_GetWindowParent(ctx.window);
        //if (sdlParent) {
        //    // It has an SDL parent
        //    std::cerr << "[SDLContext] parent failed: " << SDL_GetWindowParent(ctx.window) << "\n";
        //}

        return true;

        //std::cout << "SDL version: " << SDL_GetRevision()
        //    << " " << SDL_GetVersion() << "\n";

        //int result = SDL_Init(SDL_INIT_VIDEO);
        //if (result < 0) {
        //    // Handle error
        //    throw std::runtime_error(SDL_GetError());
        //}


        //HWND hwnd = almondnamespace::contextwindow::WindowData::getWindowHandle();
        //SDL_PropertiesID props = SDL_CreateProperties();

        //if (!props)
        //    throw std::runtime_error("Failed to create SDL properties");

        //SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "AlmondEngine");
        //SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, s_sdlstate.window.get_width());
        //SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, s_sdlstate.window.get_height());
        //SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);

        //// 👉 If you *already* have HWND:
        //if (hwnd) {
        //    SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, hwnd);
        //    // SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_PARENT_POINTER, hwnd);
        //}

        //// reposition after load is the best we can do with HWND (WinMain)
        //// but this cures the issue of SDL not centering the window
        //// it also cures the issue of SDL using winmain and not breaking it with it's titlebar formatting
        //s_sdlstate.window.sdl_window = SDL_CreateWindowWithProperties(props);
        //SDL_SetWindowSize(s_sdlstate.window.sdl_window, s_sdlstate.window.get_width(), s_sdlstate.window.get_height());
        ////SDL_SetWindowPosition(s_sdlstate.window.sdl_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

        //SDL_DestroyProperties(props);

        //if (!s_sdlstate.window.sdl_window)
        //    throw std::runtime_error(SDL_GetError());

        //sdl_renderer.renderer = SDL_CreateRenderer(s_sdlstate.window.sdl_window, nullptr);
        //if (!sdl_renderer.renderer)
        //{
        //    SDL_DestroyWindow(s_sdlstate.window.sdl_window);
        //    SDL_Quit();
        //    throw std::runtime_error(SDL_GetError());
        //}

        //SDL_SetRenderDrawColor(sdl_renderer.renderer, 0, 0, 0, 255);
        ////initialize_with_sdl_window();
        //return true;
    }

    inline bool sdl_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue) {
        (void)ctx;
        if (!sdlcontext.running || !sdlcontext.renderer) {
            return false;
        }

        atlasmanager::process_pending_uploads(core::ContextType::SDL);

        SDL_Event e;
        const bool* keys = SDL_GetKeyboardState(nullptr);
        static int escapeCount = 0;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                sdlcontext.running = false;
            }
            if (keys && keys[SDL_SCANCODE_ESCAPE]) {
                sdlcontext.running = false;
                std::cout << "Escape Key Event Count: " << ++escapeCount << '\n';
            }
        }

        if (!sdlcontext.running) {
            return false;
        }

        static auto* bgTimer = almondnamespace::time::getTimer("menu", "bg_color");
        if (!bgTimer)
            bgTimer = &almondnamespace::time::createNamedTimer("menu", "bg_color");

        double t = almondnamespace::time::elapsed(*bgTimer); // seconds

        // smooth rainbow values
        // Uint8 r = static_cast<Uint8>((0.5 + 0.5 * std::min(t * 1.0)) * 255);
        // Uint8 g = static_cast<Uint8>((0.5 + 0.5 * std::min(t * 0.7 + 2.0)) * 255);
        // Uint8 b = static_cast<Uint8>((0.5 + 0.5 * std::min(t * 1.3 + 4.0)) * 255);

        refresh_dimensions(ctx);

        SDL_SetRenderDrawColor(sdl_renderer.renderer, 124, 0, 255, 255);
        SDL_RenderClear(sdl_renderer.renderer);

        queue.drain();

        SDL_RenderPresent(sdl_renderer.renderer);
        return true;
    }

    //inline void clear()
//{
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//}

//inline void present()
//{
//    SDL_GL_SwapWindow(s_state.window.sdl_window);
//}

    inline void sdl_clear()
    {
        SDL_SetRenderDrawColor(sdl_renderer.renderer, 0, 0, 0, 255);
        SDL_RenderClear(sdl_renderer.renderer);
    }

    inline void sdl_present()
    {
        SDL_RenderPresent(sdl_renderer.renderer);
    }

    inline void sdl_cleanup(std::shared_ptr<almondnamespace::core::Context>& ctx) {
        if (sdlcontext.renderer) {
            SDL_DestroyRenderer(sdlcontext.renderer);
            sdlcontext.renderer = nullptr;
        }
        if (sdlcontext.window) {
            SDL_DestroyWindow(sdlcontext.window);
            sdlcontext.window = nullptr;
        }
        SDL_Quit();
        sdlcontext.running = false;
        //SDL_GL_DestroyContext(s_sdlstate.window.sdl_glrc);
        //SDL_DestroyWindow(s_sdlstate.window.sdl_window);
        //SDL_Quit();
    }

    inline void set_window_position_centered()
    {
        if (s_sdlstate.window.sdl_window)
        {
            int w, h;
            SDL_GetWindowSize(s_sdlstate.window.sdl_window, &w, &h);
            SDL_SetWindowPosition(s_sdlstate.window.sdl_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        }
    }

    inline void set_window_position(int x, int y)
    {
        if (s_sdlstate.window.sdl_window)
            SDL_SetWindowPosition(s_sdlstate.window.sdl_window, x, y);
    }
    inline void set_window_fullscreen(bool fullscreen)
    {
        if (s_sdlstate.window.sdl_window)
        {
            if (fullscreen)
                SDL_SetWindowFullscreen(s_sdlstate.window.sdl_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            else
                SDL_SetWindowFullscreen(s_sdlstate.window.sdl_window, 0);
        }
    }
    inline void set_window_borderless(bool borderless)
    {
        if (s_sdlstate.window.sdl_window)
        {
            SDL_SetWindowBordered(s_sdlstate.window.sdl_window, !borderless);
        }
    }
    inline void set_window_is_resizable(bool resizable)
    {
        if (s_sdlstate.window.sdl_window)
        {
            if (resizable)
                SDL_SetWindowResizable(s_sdlstate.window.sdl_window, true);
            else
                SDL_SetWindowResizable(s_sdlstate.window.sdl_window, false);
        }
    }
    inline void set_window_minimized()
    {
        if (s_sdlstate.window.sdl_window)
        {
            SDL_MinimizeWindow(s_sdlstate.window.sdl_window);
        }
    }
    inline void set_window_size(int width, int height)
    {
        if (s_sdlstate.window.sdl_window)
            SDL_SetWindowSize(s_sdlstate.window.sdl_window, width, height);
	}

    inline void sdl_set_window_title(const std::string& title)
    {
        if (s_sdlstate.window.sdl_window)
            SDL_SetWindowTitle(s_sdlstate.window.sdl_window, title.c_str());
	}

    inline std::pair<int, int> get_size() noexcept
    {
        int w = sdlcontext.width;
        int h = sdlcontext.height;

        if (sdlcontext.window)
        {
            SDL_GetWindowSize(sdlcontext.window, &w, &h);
        }

        w = (std::max)(1, w);
        h = (std::max)(1, h);
        return { w, h };
    }

    inline int sdl_get_width() noexcept { return (std::max)(1, sdlcontext.width); }
    inline int sdl_get_height() noexcept { return (std::max)(1, sdlcontext.height); }

    inline bool SDLIsRunning(std::shared_ptr<core::Context> ctx) {
        return sdlcontext.running;
    }

}

#endif // ALMOND_USING_SDL
