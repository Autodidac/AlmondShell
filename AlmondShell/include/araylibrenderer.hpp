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
 // araylibrenderer.hpp
#pragma once

#include "aplatform.hpp"        // Must be first
#include "aengineconfig.hpp"

#if defined(ALMOND_USING_RAYLIB)

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "araylibtextures.hpp"
#include "aspritehandle.hpp"
#include "aatlastexture.hpp"
#include "araylibstate.hpp"

#include <algorithm>
#include <span>
#include <utility>

#include <raylib.h> // required

namespace almondnamespace::raylibcontext
{
    // ---------- DPI / framebuffer helpers (header-inline for MSVC/linker) ----------

    // Returns (pixels_per_logical_x, pixels_per_logical_y)
    inline std::pair<float, float> framebuffer_scale() noexcept
    {
#if !defined(RAYLIB_NO_WINDOW)
        if (IsWindowReady()) {
            const int rw = (std::max)(1, GetRenderWidth());   // framebuffer pixels
            const int rh = (std::max)(1, GetRenderHeight());
            const int sw = (std::max)(1, GetScreenWidth());   // logical window units
            const int sh = (std::max)(1, GetScreenHeight());
            return { float(rw) / float(sw), float(rh) / float(sh) };
        }
#endif
        return { 1.0f, 1.0f };
    }

    inline float ui_scale_x() noexcept {
#if !defined(RAYLIB_NO_WINDOW)
        if (IsWindowReady()) {
            const Vector2 dpi = GetWindowScaleDPI();           // e.g. {1.5, 1.5}
            if (dpi.x > 0.f) return dpi.x;
        }
#endif
        return 1.f;
    }

    inline float ui_scale_y() noexcept {
#if !defined(RAYLIB_NO_WINDOW)
        if (IsWindowReady()) {
            const Vector2 dpi = GetWindowScaleDPI();
            if (dpi.y > 0.f) return dpi.y;
        }
#endif
        return 1.f;
    }

    // Call this after init and whenever size/DPI can change
    inline void sync_input_scaling() noexcept {
#if !defined(RAYLIB_NO_WINDOW)
        if (!IsWindowReady()) return;
        const auto [sx, sy] = framebuffer_scale();
        SetMouseScale(sx, sy);
#endif
    }

    // ---------- Renderer state ----------
    struct RendererContext
    {
        enum class RenderMode { SingleTexture, TextureAtlas };
        RenderMode mode = RenderMode::TextureAtlas;
    };

    inline RendererContext raylib_renderer{};

    inline void begin_frame()
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        // Viewport is set in your context loop (raylib_process); keep it there.
    }

    inline void end_frame()
    {
        EndDrawing();
    }

    // Draw sprite in either normalized (0..1) or logical pixels (pre-DPI) space.
    inline void draw_sprite(SpriteHandle handle,
        std::span<const TextureAtlas* const> atlases,
        float x, float y, float width, float height) noexcept
    {
        if (!handle.is_valid()) return;

        const int a = (int)handle.atlasIndex;
        const int i = (int)handle.localIndex;
        if (a < 0 || a >= (int)atlases.size()) return;

        const TextureAtlas* atlas = atlases[a];
        if (!atlas) return;

        AtlasRegion r{};
        if (!atlas->try_get_entry_info(i, r)) return;

        // Ensure GPU upload
        almondnamespace::raylibtextures::ensure_uploaded(*atlas);
        auto it = almondnamespace::raylibtextures::raylib_gpu_atlases.find(atlas);
        if (it == almondnamespace::raylibtextures::raylib_gpu_atlases.end() || it->second.texture.id == 0) return;

        const Texture2D& tex = it->second.texture;

        // Source rect in atlas pixels
        Raylib_Rectangle src{ (float)r.x, (float)r.y, (float)r.width, (float)r.height };

        const GuiFitViewport fit = s_raylibstate.lastViewport;
        const float viewportWidth = (float)(std::max)(1, fit.vpW);
        const float viewportHeight = (float)(std::max)(1, fit.vpH);
        const float viewportScale = (fit.scale > 0.0f) ? fit.scale : 1.0f;
        const float designWidth = (float)(std::max)(1, fit.refW);
        const float designHeight = (float)(std::max)(1, fit.refH);

#if !defined(RAYLIB_NO_WINDOW)
        Vector2 offset = { 0.0f, 0.0f };
        if (IsWindowReady()) {
            offset = GetRenderOffset();
        }
#else
        Vector2 offset = { 0.0f, 0.0f };
#endif

        const float baseOffsetX = offset.x + static_cast<float>(fit.vpX);
        const float baseOffsetY = offset.y + static_cast<float>(fit.vpY);

        // Consider rect "normalized" if any dimension is 0<..<=1 or coords are in [0..1]
        const bool normalized =
            (x >= 0.f && x <= 1.f && y >= 0.f && y <= 1.f) &&
            ((width <= 1.f && width >= 0.f) || width <= 0.f) &&
            ((height <= 1.f && height >= 0.f) || height <= 0.f);

        float px, py, pw, ph;
        if (normalized) {
            // 0..1 -> viewport pixels
            px = baseOffsetX + x * designWidth * viewportScale;
            py = baseOffsetY + y * designHeight * viewportScale;
            const float scaledW = (width > 0.f ? width * designWidth * viewportScale : (float)r.width * viewportScale);
            const float scaledH = (height > 0.f ? height * designHeight * viewportScale : (float)r.height * viewportScale);
            pw = (std::max)(scaledW, 1.0f);
            ph = (std::max)(scaledH, 1.0f);
        }
        else {
            // logical pixels -> viewport-scaled pixels
            px = baseOffsetX + x * viewportScale;
            py = baseOffsetY + y * viewportScale;
            const float scaledW = (width > 0.f ? width * viewportScale : (float)r.width * viewportScale);
            const float scaledH = (height > 0.f ? height * viewportScale : (float)r.height * viewportScale);
            pw = (std::max)(scaledW, 1.0f);
            ph = (std::max)(scaledH, 1.0f);
        }

        Raylib_Rectangle dst{ px, py, pw, ph };
        DrawTexturePro(tex, src, dst, Vector2{ 0,0 }, 0.0f, WHITE);
    }

} // namespace almondnamespace::raylibcontext

#endif // ALMOND_USING_RAYLIB
