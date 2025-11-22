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
 // asfmlcontext.hpp
#pragma once

#include "aengineconfig.hpp"

#if defined(ALMOND_USING_SFML)

#include "aplatform.hpp"
#include "asfmlstate.hpp"

#include "acontext.hpp"
#include "acontextwindow.hpp"
#include "acommandline.hpp"
#include "asfmltextures.hpp"
#include "aatlasmanager.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <functional>
#include <cmath>

namespace almondnamespace::sfmlcontext
{
    using namespace almondnamespace::contextwindow;
    using namespace almondnamespace::core;

    struct SFMLState {
        std::unique_ptr<sf::RenderWindow> window;
        HWND parent = nullptr;
        HWND hwnd = nullptr;
        HDC hdc = nullptr;
        HGLRC glContext = nullptr;
        unsigned int width = 400;
        unsigned int height = 300;
        bool running = false;
        std::function<void(int, int)> onResize;
    };

    inline SFMLState sfmlcontext;

    inline bool sfml_initialize(std::shared_ptr<core::Context> ctx,
        HWND parentWnd = nullptr,
        unsigned int w = 400,
        unsigned int h = 300,
        std::function<void(int, int)> onResize = nullptr)
    {
        ctx->onResize = std::move(onResize);
        sfmlcontext.width = w;
        sfmlcontext.height = h;
        sfmlcontext.parent = parentWnd;

        sf::ContextSettings settings;
        settings.majorVersion = 3;
        settings.minorVersion = 3;
        settings.attributeFlags = sf::ContextSettings::Core;

        sf::VideoMode mode({ sfmlcontext.width, sfmlcontext.height }, 32);
        sfmlcontext.window = std::make_unique<sf::RenderWindow>(
            mode, "SFML Window", sf::State::Windowed, settings);

        if (!sfmlcontext.window || !sfmlcontext.window->isOpen()) {
            std::cerr << "[SFML] Failed to create SFML window\n";
            return false;
        }

        sfmlcontext.hwnd = sfmlcontext.window->getNativeHandle();
        sfmlcontext.hdc = GetDC(sfmlcontext.hwnd);
        sfmlcontext.glContext = wglGetCurrentContext();
        if (!sfmlcontext.glContext) {
            std::cerr << "[SFML] Failed to get OpenGL context\n";
            return false;
        }

        if (sfmlcontext.parent) {
            SetParent(sfmlcontext.hwnd, sfmlcontext.parent);
            LONG_PTR style = GetWindowLongPtr(sfmlcontext.hwnd, GWL_STYLE);
            style &= ~WS_OVERLAPPEDWINDOW;
            style |= WS_CHILD | WS_VISIBLE;
            SetWindowLongPtr(sfmlcontext.hwnd, GWL_STYLE, style);

            RECT client{};
            GetClientRect(sfmlcontext.parent, &client);
            const int width = static_cast<int>((std::max)(static_cast<LONG>(1), client.right - client.left));
            const int height = static_cast<int>((std::max)(static_cast<LONG>(1), client.bottom - client.top));

            sfmlcontext.width = width;
            sfmlcontext.height = height;

            SetWindowPos(sfmlcontext.hwnd, nullptr, 0, 0,
                width, height,
                SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

            if (sfmlcontext.onResize)
                sfmlcontext.onResize(width, height);
        }

        std::cout << "[SFML] HWND=" << sfmlcontext.hwnd
            << " HDC=" << sfmlcontext.hdc
            << " HGLRC=" << sfmlcontext.glContext << "\n";

        sfmlcontext.running = true;

        atlasmanager::register_backend_uploader(core::ContextType::SFML,
            [](const TextureAtlas& atlas) {
                sfmlcontext::ensure_uploaded(atlas);
            });

        return true;
    }

    inline void sfml_begin_frame()
    {
        if (!sfmlcontext.window)
            throw std::runtime_error("[SFML] Window not initialized.");
        sfmlcontext.window->clear(sf::Color::Black);
    }

    inline void sfml_end_frame()
    {
        if (!sfmlcontext.window)
            throw std::runtime_error("[SFML] Window not initialized.");
        sfmlcontext.window->display();
    }

    inline void sfml_clear() { sfml_begin_frame(); }
    inline void sfml_present() { sfml_end_frame(); }

    inline bool sfml_should_close()
    {
        return !sfmlcontext.window || !sfmlcontext.window->isOpen();
    }

    inline bool sfml_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue)
    {
        if (!sfmlcontext.running || !sfmlcontext.window || !sfmlcontext.window->isOpen())
            return false;

        atlasmanager::process_pending_uploads(core::ContextType::SFML);

        if (!wglMakeCurrent(sfmlcontext.hdc, sfmlcontext.glContext)) {
            std::cerr << "[SFMLRender] Failed to activate GL context\n";
            sfmlcontext.running = false;
            return false;
        }

        if (!sfmlcontext.window->setActive(true)) {
            std::cerr << "[SFMLRender] Failed to activate SFML window\n";
            sfmlcontext.running = false;
            wglMakeCurrent(nullptr, nullptr);
            return false;
        }

        while (auto event = sfmlcontext.window->pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                sfmlcontext.window->close();
                sfmlcontext.running = false;
            }
        }

        if (!sfmlcontext.running) {
            (void)sfmlcontext.window->setActive(false);
            (void)wglMakeCurrent(nullptr, nullptr);

            return false;
        }

        static auto* bgTimer = almondnamespace::time::getTimer("menu", "bg_color");
        if (!bgTimer)
            bgTimer = &almondnamespace::time::createNamedTimer("menu", "bg_color");

        double t = almondnamespace::time::elapsed(*bgTimer);
        unsigned char r = static_cast<unsigned char>((0.5 + 0.5 * std::sin(t * 1.0)) * 255);
        unsigned char g = static_cast<unsigned char>((0.5 + 0.5 * std::sin(t * 0.7 + 2.0)) * 255);
        unsigned char b = static_cast<unsigned char>((0.5 + 0.5 * std::sin(t * 1.3 + 4.0)) * 255);

        sfmlcontext.window->clear(sf::Color(r, g, b));
        sfmlcontext.window->display();

        (void)sfmlcontext.window->setActive(false);
        (void)wglMakeCurrent(nullptr, nullptr);

        return sfmlcontext.running;
    }

    inline std::pair<int, int> get_window_size_wh() noexcept
    {
        if (!sfmlcontext.window) return { 0, 0 };
        auto size = sfmlcontext.window->getSize();
        return { static_cast<int>(size.x), static_cast<int>(size.y) };
    }

    inline std::pair<int, int> get_window_position_xy() noexcept
    {
        if (!sfmlcontext.window) return { 0, 0 };
        auto pos = sfmlcontext.window->getPosition();
        return { pos.x, pos.y };
    }

    inline void set_window_position(int x, int y) noexcept
    {
        if (sfmlcontext.window)
            sfmlcontext.window->setPosition(sf::Vector2i(x, y));
    }

    inline void set_window_size(int width, int height) noexcept
    {
        if (sfmlcontext.window)
            sfmlcontext.window->setSize(sf::Vector2u(width, height));
    }

    inline void set_window_icon(const std::string& iconPath) noexcept
    {
        if (!sfmlcontext.window) return;
        // implement with sf::Image if desired
    }

    inline int sfml_get_width() noexcept
    {
        return sfmlcontext.window ? static_cast<int>(sfmlcontext.window->getSize().x) : 0;
    }

    inline int sfml_get_height() noexcept
    {
        return sfmlcontext.window ? static_cast<int>(sfmlcontext.window->getSize().y) : 0;
    }

    inline void sfml_set_window_title(const std::string& title) noexcept
    {
        if (sfmlcontext.window)
            sfmlcontext.window->setTitle(title);
    }

    inline void sfml_cleanup(std::shared_ptr<almondnamespace::core::Context>& ctx)
    {
        if (sfmlcontext.running && sfmlcontext.window) {
            sfmlcontext.window->close();
            sfmlcontext.window.reset();
            sfmlcontext.running = false;
        }
    }

    inline bool SFMLIsRunning(std::shared_ptr<core::Context> ctx)
    {
        return sfmlcontext.running;
    }
}

#endif // ALMOND_USING_SFML
