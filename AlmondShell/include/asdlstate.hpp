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
#pragma once

//#include "aplatform.hpp"
#include "aengineconfig.hpp"

#if defined(ALMOND_USING_SDL)

//#include "arobusttime.hpp"
#include "acontextwindow.hpp"


namespace almondnamespace::sdlcontext::state
{
    struct SDL3State
    {
        almondnamespace::contextwindow::WindowData window{};

        SDL_Event sdl_event{};

        struct MouseState
        {
            bool down[5] = {};
            bool pressed[5] = {};
            int lastX = 0;
            int lastY = 0;
        } mouse;

        struct KeyboardState
        {
            bool down[SDL_SCANCODE_COUNT] = {};
            bool pressed[SDL_SCANCODE_COUNT] = {};
        } keyboard;

#ifdef _WIN32
    private:
        WNDPROC oldWndProc_ = nullptr;

    public:
        WNDPROC getOldWndProc() const noexcept { return oldWndProc_; }
        void setOldWndProc(WNDPROC proc) noexcept { oldWndProc_ = proc; }
#endif

        HWND hwnd() const { return window.hwnd; }
        HDC hdc() const { return window.hdc; }
        HGLRC glrc() const { return window.glrc; }
        SDL_Window* sdl_window() const { return window.sdl_window; }
    };

    inline SDL3State s_sdlstate{};
}

#endif // ALMOND_USING_SDL
