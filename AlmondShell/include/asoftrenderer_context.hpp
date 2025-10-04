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
 // SoftRenderer - Context / Quad Backend
#pragma once

#include "aplatform.hpp"
#include "aengineconfig.hpp"

#if defined(ALMOND_USING_SOFTWARE_RENDERER)

#include "acontext.hpp"
#include "asoftrenderer_state.hpp"
#include "asoftrenderer_textures.hpp"
#include "asoftrenderer_renderer.hpp"
#include "aatlasmanager.hpp"

#include <memory>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>
#include <algorithm>
#undef max

#ifdef _WIN32
#include <windows.h>
#endif

namespace almondnamespace::anativecontext
{
    inline TexturePtr cubeTexture;
    inline SoftwareRenderer renderer;

    inline void resize_framebuffer(int width, int height)
    {
        auto& sr = s_softrendererstate;

        const int clampedWidth = std::max(1, width);
        const int clampedHeight = std::max(1, height);
        const size_t requiredSize = static_cast<size_t>(clampedWidth) * static_cast<size_t>(clampedHeight);

        if (sr.width == clampedWidth && sr.height == clampedHeight && sr.framebuffer.size() == requiredSize)
        {
            return;
        }

        sr.width = clampedWidth;
        sr.height = clampedHeight;
        sr.framebuffer.assign(requiredSize, 0xFF000000);

#ifdef _WIN32
        sr.bmi.bmiHeader.biWidth = clampedWidth;
        sr.bmi.bmiHeader.biHeight = -clampedHeight; // top-down
#endif
    }

    // Initialize the software renderer
    inline bool softrenderer_initialize(std::shared_ptr<core::Context> ctx,
        HWND parentWnd = nullptr,
        unsigned int w = 400, unsigned int h = 300,
        std::function<void(int, int)> onResize = nullptr)
    {
        if (!ctx || !ctx->hwnd) {
            std::cerr << "[SoftRenderer] Invalid context hwnd\n";
            return false;
        }

        resize_framebuffer(static_cast<int>(w), static_cast<int>(h));
        s_softrendererstate.running = true;
        s_softrendererstate.parent = parentWnd ? parentWnd : ctx->hwnd;
        s_softrendererstate.onResize = std::move(onResize);
        s_softrendererstate.hwnd = ctx->hwnd;
        s_softrendererstate.hdc = ctx->hdc;

        if (ctx)
        {
            ctx->onResize = [](int newWidth, int newHeight)
            {
                resize_framebuffer(newWidth, newHeight);

                if (s_softrendererstate.onResize)
                {
                    s_softrendererstate.onResize(newWidth, newHeight);
                }
            };
        }

        if (!cubeTexture) {
            cubeTexture = std::make_shared<Texture>(64, 64);
            for (int y = 0; y < cubeTexture->height; ++y) {
                for (int x = 0; x < cubeTexture->width; ++x) {
                    cubeTexture->pixels[size_t(y) * size_t(cubeTexture->width) + size_t(x)] =
                        ((x / 8 + y / 8) % 2) ? 0xFFFF0000 : 0xFF00FF00; // checkerboard
                }
            }
        }

#ifdef _WIN32
        ZeroMemory(&s_softrendererstate.bmi, sizeof(BITMAPINFO));
        s_softrendererstate.bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        s_softrendererstate.bmi.bmiHeader.biWidth = static_cast<int>(w);
        s_softrendererstate.bmi.bmiHeader.biHeight = -static_cast<int>(h); // top-down
        s_softrendererstate.bmi.bmiHeader.biPlanes = 1;
        s_softrendererstate.bmi.bmiHeader.biBitCount = 32;
        s_softrendererstate.bmi.bmiHeader.biCompression = BI_RGB;
#endif

        std::cout << "[SoftRenderer] Initialized on HWND=" << ctx->hwnd << "\n";

        atlasmanager::register_backend_uploader(core::ContextType::Software,
            [](const TextureAtlas& atlas) {
                if (atlas.pixel_data.empty()) {
                    const_cast<TextureAtlas&>(atlas).rebuild_pixels();
                }
            });

        return true;
    }

    // Draw a textured quad
    inline void softrenderer_draw_quad(SoftRendState& softstate)
    {
        if (atlasmanager::atlas_vector.empty()) return;
        const auto* atlas = atlasmanager::atlas_vector[0];
        if (!atlas) return;

        int w = atlas->width;
        int h = atlas->height;
        if (w <= 0 || h <= 0 || atlas->pixel_data.empty()) return;

        for (int y = 0; y < softstate.height; ++y) {
            for (int x = 0; x < softstate.width; ++x) {
                const float u = float(x) / float(std::max(1, softstate.width));
                const float v = float(y) / float(std::max(1, softstate.height));

                const int texX = static_cast<int>(u * float(w - 1));
                const int texY = static_cast<int>(v * float(h - 1));

                const size_t byteIndex = (static_cast<size_t>(texY) * static_cast<size_t>(w)
                    + static_cast<size_t>(texX))
                    * 4;
                if (byteIndex + 3 >= atlas->pixel_data.size()) {
                    throw std::runtime_error("[Software] Pixel idx out of range");
                }

                const uint8_t* src = atlas->pixel_data.data() + byteIndex;
                const uint32_t color = (uint32_t(src[3]) << 24)
                    | (uint32_t(src[0]) << 16)
                    | (uint32_t(src[1]) << 8)
                    | uint32_t(src[2]);

                softstate.framebuffer[size_t(y) * size_t(softstate.width) + size_t(x)] = color;
            }
        }
    }

    inline void draw_sprite(SpriteHandle handle,
        std::span<const TextureAtlas* const> atlases,
        float x, float y, float width, float height) noexcept
    {
        if (!handle.is_valid()) {
            std::cerr << "[Software_DrawSprite] Invalid sprite handle.\n";
            return;
        }

        const int atlasIdx = static_cast<int>(handle.atlasIndex);
        const int localIdx = static_cast<int>(handle.localIndex);

        if (atlasIdx < 0 || atlasIdx >= static_cast<int>(atlases.size())) {
            std::cerr << "[Software_DrawSprite] Atlas index out of range: " << atlasIdx << "\n";
            return;
        }

        const TextureAtlas* atlas = atlases[atlasIdx];
        if (!atlas) {
            std::cerr << "[Software_DrawSprite] Null atlas pointer for index " << atlasIdx << "\n";
            return;
        }

        AtlasRegion region{};
        if (!atlas->try_get_entry_info(localIdx, region)) {
            std::cerr << "[Software_DrawSprite] Sprite index out of range: " << localIdx << "\n";
            return;
        }

        if (atlas->pixel_data.empty()) {
            const_cast<TextureAtlas*>(atlas)->rebuild_pixels();
        }

        auto& sr = s_softrendererstate;
        if (sr.framebuffer.empty() || sr.width <= 0 || sr.height <= 0) {
            return;
        }

        float drawX = x;
        float drawY = y;
        float drawWidth = width;
        float drawHeight = height;

        const bool widthNormalized = drawWidth > 0.f && drawWidth <= 1.f;
        const bool heightNormalized = drawHeight > 0.f && drawHeight <= 1.f;

        if (widthNormalized) {
            if (drawX >= 0.f && drawX <= 1.f)
                drawX *= static_cast<float>(sr.width);
            drawWidth = std::max(drawWidth * static_cast<float>(sr.width), 1.0f);
        }
        if (heightNormalized) {
            if (drawY >= 0.f && drawY <= 1.f)
                drawY *= static_cast<float>(sr.height);
            drawHeight = std::max(drawHeight * static_cast<float>(sr.height), 1.0f);
        }

        if (drawWidth <= 0.f)
            drawWidth = static_cast<float>(region.width);
        if (drawHeight <= 0.f)
            drawHeight = static_cast<float>(region.height);

        const int destX = static_cast<int>(std::floor(drawX));
        const int destY = static_cast<int>(std::floor(drawY));
        const int destW = std::max(1, static_cast<int>(std::lround(drawWidth)));
        const int destH = std::max(1, static_cast<int>(std::lround(drawHeight)));

        const int clipX0 = std::max(0, destX);
        const int clipY0 = std::max(0, destY);
        const int clipX1 = std::min(sr.width, destX + destW);
        const int clipY1 = std::min(sr.height, destY + destH);
        if (clipX0 >= clipX1 || clipY0 >= clipY1) {
            return;
        }

        const int srcW = static_cast<int>(std::max<uint32_t>(1u, region.width));
        const int srcH = static_cast<int>(std::max<uint32_t>(1u, region.height));
        const float invDestW = 1.0f / static_cast<float>(destW);
        const float invDestH = 1.0f / static_cast<float>(destH);

        for (int py = clipY0; py < clipY1; ++py) {
            const float v = (py - destY) * invDestH;
            const int sampleY = std::clamp(static_cast<int>(std::floor(v * srcH)), 0, srcH - 1);
            for (int px = clipX0; px < clipX1; ++px) {
                const float u = (px - destX) * invDestW;
                const int sampleX = std::clamp(static_cast<int>(std::floor(u * srcW)), 0, srcW - 1);

                const int atlasX = static_cast<int>(region.x) + sampleX;
                const int atlasY = static_cast<int>(region.y) + sampleY;
                const size_t srcIndex = (static_cast<size_t>(atlasY) * atlas->width + static_cast<size_t>(atlasX)) * 4;
                if (srcIndex + 3 >= atlas->pixel_data.size()) {
                    continue;
                }

                const uint8_t* src = atlas->pixel_data.data() + srcIndex;
                const uint32_t color = (uint32_t(src[3]) << 24)
                    | (uint32_t(src[0]) << 16)
                    | (uint32_t(src[1]) << 8)
                    | uint32_t(src[2]);

                sr.framebuffer[static_cast<size_t>(py) * static_cast<size_t>(sr.width) + static_cast<size_t>(px)] = color;
            }
        }
    }

    // Main process loop
    inline bool softrenderer_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue) {
        auto& sr = s_softrendererstate;

        atlasmanager::process_pending_uploads(core::ContextType::Software);

        const size_t expectedSize = static_cast<size_t>(std::max(1, sr.width)) * static_cast<size_t>(std::max(1, sr.height));
        if (sr.framebuffer.size() != expectedSize) {
            resize_framebuffer(sr.width, sr.height);
        }

        // Clear framebuffer
        std::fill(sr.framebuffer.begin(), sr.framebuffer.end(), 0xFF000000);

        // Drain queued commands
        queue.drain();

#ifdef _WIN32
        // Present framebuffer to window
        HDC hdc = ctx->hdc;
        if (hdc) {
            BITMAPINFO bmi{};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = sr.width;
            bmi.bmiHeader.biHeight = -sr.height; // top-down
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            StretchDIBits(hdc,
                0, 0, sr.width, sr.height,
                0, 0, sr.width, sr.height,
                sr.framebuffer.data(),
                &bmi, DIB_RGB_COLORS, SRCCOPY);
        }
#endif
        return true;
    }

    inline void softrenderer_cleanup(std::shared_ptr<core::Context> ctx)
    {
        auto& sr = s_softrendererstate;

        sr.framebuffer.clear();
        cubeTexture.reset();
#ifdef _WIN32
        if (s_softrendererstate.hwnd) {
            DestroyWindow(s_softrendererstate.hwnd);
            s_softrendererstate.hwnd = nullptr;
        }
#endif
        s_softrendererstate = {};
        std::cout << "[SoftRenderer] Cleanup complete\n";
    }

    inline int get_width() { return s_softrendererstate.width; }
    inline int get_height() { return s_softrendererstate.height; }
}

#endif // ALMOND_USING_SOFTWARE_RENDERER
