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
// aopenglplatform.hpp
#pragma once

#include "aplatform.hpp"
#include "aengineconfig.hpp"

#if defined(ALMOND_USING_OPENGL)

#if defined(_WIN32)
#    include <windows.h>
#elif defined(__linux__)
// X11 defines a typedef named `Font` that clashes with the global `Font`
// type exported by raylib. Raylib is included from aengineconfig.hpp before
// we reach this point, so temporarily remap the X11 `Font` symbol while the
// header is processed to avoid the conflicting typedef.
#    pragma push_macro("Font")
#    define Font almondshell_X11Font
#    include <X11/Xlib.h>
#    include <GL/glx.h>
#    pragma pop_macro("Font")
#endif

#include <utility>

namespace almondnamespace::openglcontext::PlatformGL
{
    struct PlatformGLContext
    {
#if defined(_WIN32)
        HDC device = nullptr;
        HGLRC context = nullptr;
#elif defined(__linux__)
        Display* display = nullptr;
        GLXDrawable drawable = 0;
        GLXContext context = nullptr;
#else
        void* placeholder = nullptr;
#endif

        [[nodiscard]] bool valid() const noexcept
        {
#if defined(_WIN32)
            return device != nullptr && context != nullptr;
#elif defined(__linux__)
            return display != nullptr && drawable != 0 && context != nullptr;
#else
            return false;
#endif
        }
    };

#if defined(_WIN32) || defined(__linux__)
    inline bool operator==(const PlatformGLContext& lhs, const PlatformGLContext& rhs) noexcept
    {
#if defined(_WIN32)
        return lhs.device == rhs.device && lhs.context == rhs.context;
#elif defined(__linux__)
        return lhs.display == rhs.display && lhs.drawable == rhs.drawable && lhs.context == rhs.context;
#else
        return false;
#endif
    }

    inline bool operator!=(const PlatformGLContext& lhs, const PlatformGLContext& rhs) noexcept
    {
        return !(lhs == rhs);
    }
#endif

    PlatformGLContext get_current() noexcept;
    bool make_current(const PlatformGLContext& ctx) noexcept;
    void clear_current() noexcept;
    void swap_buffers(const PlatformGLContext& ctx) noexcept;
    void* get_proc_address(const char* name) noexcept;

    class ScopedContext
    {
    public:
        ScopedContext() = default;
        explicit ScopedContext(const PlatformGLContext& target)
        {
            set(target);
        }

        ScopedContext(const ScopedContext&) = delete;
        ScopedContext& operator=(const ScopedContext&) = delete;

        ScopedContext(ScopedContext&& other) noexcept
        {
            move_from(other);
        }

        ScopedContext& operator=(ScopedContext&& other) noexcept
        {
            if (this != &other)
            {
                release();
                move_from(other);
            }
            return *this;
        }

        ~ScopedContext()
        {
            release();
        }

        bool set(const PlatformGLContext& target) noexcept
        {
            release();
            target_ = target;
            if (!target.valid())
            {
                success_ = false;
                return success_;
            }

            previous_ = get_current();
            if (previous_ == target_)
            {
                success_ = true;
                switched_ = false;
                return success_;
            }

            switched_ = make_current(target_);
            success_ = switched_;
            return success_;
        }

        void release() noexcept
        {
            if (switched_)
            {
                if (previous_.valid())
                {
                    make_current(previous_);
                }
                else
                {
                    clear_current();
                }
            }

            switched_ = false;
            success_ = false;
            previous_ = {};
            target_ = {};
        }

        [[nodiscard]] bool success() const noexcept { return success_; }
        [[nodiscard]] bool switched() const noexcept { return switched_; }
        [[nodiscard]] const PlatformGLContext& target() const noexcept { return target_; }

    private:
        void move_from(ScopedContext& other) noexcept
        {
            previous_ = other.previous_;
            target_ = other.target_;
            switched_ = other.switched_;
            success_ = other.success_;
            other.switched_ = false;
            other.success_ = false;
            other.previous_ = {};
            other.target_ = {};
        }

        PlatformGLContext previous_{};
        PlatformGLContext target_{};
        bool switched_ = false;
        bool success_ = false;
    };
}

#endif // ALMOND_USING_OPENGL
