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
// agui.hpp
#pragma once

#include "aplatform.hpp"
#include "aengineconfig.hpp"
#include "aspritehandle.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#ifdef None
#undef None
#endif

namespace almondnamespace::core {
    struct Context;
}

namespace almondnamespace::gui
{
    struct Vec2 {
        float x = 0.0f;
        float y = 0.0f;

        constexpr Vec2() noexcept = default;
        constexpr Vec2(float x_, float y_) noexcept : x(x_), y(y_) {}

        [[nodiscard]] constexpr Vec2 operator+(const Vec2& other) const noexcept
        {
            return Vec2{x + other.x, y + other.y};
        }

        constexpr Vec2& operator+=(const Vec2& other) noexcept
        {
            x += other.x;
            y += other.y;
            return *this;
        }
    };

    struct Color {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;
    };

    enum class EventType {
        None,
        MouseDown,
        MouseUp,
        MouseMove,
        KeyDown,
        KeyUp,
        TextInput
    };

    struct InputEvent {
        EventType type = EventType::None;
        Vec2 mouse_pos{};
        std::uint8_t mouse_button = 0;
        std::uint32_t key = 0;
        bool shift = false;
        bool ctrl = false;
        bool alt = false;
        std::string text;
    };

    void push_input(const InputEvent& e) noexcept;

    void begin_frame(const std::shared_ptr<almondnamespace::core::Context>& ctx, float dt, Vec2 mouse_pos, bool mouse_down) noexcept;
    void end_frame() noexcept;

    void begin_window(std::string_view title, Vec2 position, Vec2 size) noexcept;
    void end_window() noexcept;

    void set_cursor(Vec2 position) noexcept;
    void advance_cursor(Vec2 delta) noexcept;

    [[nodiscard]] bool button(std::string_view label, Vec2 size) noexcept;
    [[nodiscard]] bool image_button(const almondnamespace::SpriteHandle& sprite, Vec2 size) noexcept;

    struct WidgetBounds {
        Vec2 position{};
        Vec2 size{};
    };

    [[nodiscard]] std::optional<WidgetBounds> last_button_bounds() noexcept;

    struct EditBoxResult {
        bool active = false;
        bool changed = false;
        bool submitted = false;
    };

    [[nodiscard]] EditBoxResult edit_box(std::string& text, Vec2 size, std::size_t max_chars = 256, bool multiline = false) noexcept;

    void text_box(std::string_view text, Vec2 size) noexcept;

    void label(std::string_view text) noexcept;

    struct ConsoleWindowOptions {
        std::string_view title;
        Vec2 position{};
        Vec2 size{};
        std::span<const std::string> lines{};
        std::size_t max_visible_lines = 128;
        std::string* input = nullptr;
        std::size_t max_input_chars = 256;
        bool multiline_input = false;
    };

    struct ConsoleWindowResult {
        EditBoxResult input;
    };

    [[nodiscard]] ConsoleWindowResult console_window(const ConsoleWindowOptions& options) noexcept;

    [[nodiscard]] float line_height() noexcept;
    [[nodiscard]] float glyph_width() noexcept;

} // namespace almondnamespace::gui

namespace almondnamespace::ui
{
    using vec2 = gui::Vec2;
    using color = gui::Color;
    using event_type = gui::EventType;
    using input_event = gui::InputEvent;

    using gui::begin_frame;
    using gui::end_frame;
    using gui::begin_window;
    using gui::end_window;
    using gui::button;
    using gui::image_button;
    using gui::EditBoxResult;
    using gui::edit_box;
    using gui::text_box;
    using gui::label;
    using gui::set_cursor;
    using gui::advance_cursor;
    using gui::ConsoleWindowOptions;
    using gui::ConsoleWindowResult;
    using gui::console_window;
    using gui::push_input;
    using gui::line_height;
    using gui::glyph_width;
}
