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

#include "aengineconfig.hpp"

#if defined(ALMOND_USING_SFML)

#include "acontextwindow.hpp"

//#include <SFML/Graphics.hpp>
//#include <SFML/Window.hpp> // for sf::RenderWindow, sf::Event, etc.
    

namespace almondnamespace::sfmlcontext::state
{
    // This is your global renderer state
    struct SFML3State
    {
    almondnamespace::contextwindow::WindowData window{};
        // Nothing fancy needed here yet.
        // You can expand this with:
        // - active shaders
        // - viewports
        // - default font
        // - more later.

        // ✅ Safe getter, returns pointer, no UB
    [[nodiscard]] sf::RenderWindow* get_sfml_window() noexcept
    {
        return window.sfml_window;
    }

    };

    inline SFML3State s_sfmlstate{}; // for symmetry with other contexts
}

#endif // ALMOND_USING_SFML
