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
#include <atomic>

// #undef max

// #ifdef _WIN32
// #include <windows.h>
// #endif

namespace almondnamespace::anativecontext
{
    inline TexturePtr       cubeTexture;
    inline SoftwareRenderer renderer;

    // Reentrancy guard to avoid stack overflow when onResize chains back into ctx->onResize.
    inline std::atomic_bool s_isDispatchingResize{ false };
    inline std::atomic_bool s_cleanupIssued{ false };

    // If the software backend did not create the window, we shouldn't destroy it.
    inline bool s_createdOwnWindow = false; // stays false here; flip only if you ever CreateWindow in this backend.

    inline void resize_framebuffer(int width, int height)
    {
        auto& sr = s_softrendererstate;

        const int clampedWidth = (std::max)(1, width);
        const int clampedHeight = (std::max)(1, height);
        const size_t requiredSize = static_cast<size_t>(clampedWidth) * static_cast<size_t>(clampedHeight);

        // Fast no-op if nothing changed
        if (sr.width == clampedWidth && sr.height == clampedHeight && sr.framebuffer.size() == requiredSize)
            return;

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

        s_cleanupIssued.store(false, std::memory_order_release);

        resize_framebuffer(static_cast<int>(w), static_cast<int>(h));
        s_softrendererstate.running = true;
        s_softrendererstate.onResize = std::move(onResize);   // this is the *client* callback

#ifdef _WIN32
        s_softrendererstate.parent = parentWnd ? parentWnd : ctx->hwnd;
        s_softrendererstate.hwnd = ctx->hwnd;
        s_softrendererstate.hdc = ctx->hdc;
#else
        (void)parentWnd;
#endif

        // Backend-owned resize handler. Do NOT call ctx->onResize from inside this callback.
        // Guard against reentrancy so clients can safely trigger additional layout work.
// Backend-owned resize handler. Reentrancy-safe and coalescing.
        if (ctx)
        {
            ctx->onResize = [](int newWidth, int newHeight)
                {
                    // Coalesce re-entrant resize requests without recursion.
                    // We keep the *last* requested size if multiple arrive mid-dispatch.
                    struct Pending { int w, h; };
                    Pending pending{ newWidth, newHeight };

                    // If a dispatch is already in flight, stash latest and bail; the live
                    // dispatch loop below will pick it up.
                    static std::atomic_bool s_hasPending{ false };
                    static std::atomic_int  s_pendingW{ 0 };
                    static std::atomic_int  s_pendingH{ 0 };

                    if (s_isDispatchingResize.load(std::memory_order_acquire))
                    {
                        s_pendingW.store(pending.w, std::memory_order_relaxed);
                        s_pendingH.store(pending.h, std::memory_order_relaxed);
                        s_hasPending.store(true, std::memory_order_release);
                        return;
                    }

                    // Start a non-recursive dispatch loop; consume initial + any re-entrant updates.
                    s_isDispatchingResize.store(true, std::memory_order_release);
                    for (;;)
                    {
                        // 1) Internal resize
                        resize_framebuffer(pending.w, pending.h);

                        // 2) Snapshot client callback
                        auto clientCb = s_softrendererstate.onResize;

                        // 3) Notify client while guard is STILL ON (blocks re-entry).
                        if (clientCb)
                        {
                            try {
                                clientCb(pending.w, pending.h);
                            }
                            catch (const std::exception& e) {
                                std::cerr << "[SoftRenderer] onResize client callback threw: " << e.what() << "\n";
                            }
                            catch (...) {
                                std::cerr << "[SoftRenderer] onResize client callback threw unknown exception.\n";
                            }
                        }

                        // 4) Did anything request another resize during callback?
                        if (s_hasPending.exchange(false, std::memory_order_acq_rel))
                        {
                            pending.w = s_pendingW.load(std::memory_order_relaxed);
                            pending.h = s_pendingH.load(std::memory_order_relaxed);
                            // loop again with latest requested size
                            continue;
                        }

                        // 5) All caught up — release guard and exit.
                        s_isDispatchingResize.store(false, std::memory_order_release);
                        break;
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

    // Draw a textured quad from the first atlas (kept as-is)
    inline void softrenderer_draw_quad(SoftRendState& softstate)
    {
        const auto atlases = atlasmanager::get_atlas_vector_snapshot();
        if (atlases.empty()) return;
        const auto* atlas = atlases[0];
        if (!atlas) return;

        int w = atlas->width;
        int h = atlas->height;
        if (w <= 0 || h <= 0 || atlas->pixel_data.empty()) return;

        for (int y = 0; y < softstate.height; ++y) {
            for (int x = 0; x < softstate.width; ++x) {
                const float u = float(x) / float((std::max)(1, softstate.width));
                const float v = float(y) / float((std::max)(1, softstate.height));

                const int texX = static_cast<int>(u * float(w - 1));
                const int texY = static_cast<int>(v * float(h - 1));

                const size_t byteIndex = (static_cast<size_t>(texY) * static_cast<size_t>(w)
                    + static_cast<size_t>(texX)) * 4;
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
            drawWidth = (std::max)(drawWidth * static_cast<float>(sr.width), 1.0f);
        }
        if (heightNormalized) {
            if (drawY >= 0.f && drawY <= 1.f)
                drawY *= static_cast<float>(sr.height);
            drawHeight = (std::max)(drawHeight * static_cast<float>(sr.height), 1.0f);
        }

        if (drawWidth <= 0.f) drawWidth = static_cast<float>(region.width);
        if (drawHeight <= 0.f) drawHeight = static_cast<float>(region.height);

        const int destX = static_cast<int>(std::floor(drawX));
        const int destY = static_cast<int>(std::floor(drawY));
        const int destW = (std::max)(1, static_cast<int>(std::lround(drawWidth)));
        const int destH = (std::max)(1, static_cast<int>(std::lround(drawHeight)));

        const int clipX0 = (std::max)(0, destX);
        const int clipY0 = (std::max)(0, destY);
        const int clipX1 = (std::min)(sr.width, destX + destW);
        const int clipY1 = (std::min)(sr.height, destY + destH);
        if (clipX0 >= clipX1 || clipY0 >= clipY1) {
            return;
        }

        const int srcW = static_cast<int>((std::max)(1u, region.width));
        const int srcH = static_cast<int>((std::max)(1u, region.height));
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
                const size_t srcIndex =
                    (static_cast<size_t>(atlasY) * atlas->width + static_cast<size_t>(atlasX)) * 4;

                if (srcIndex + 3 >= atlas->pixel_data.size()) continue;

                const uint8_t* src = atlas->pixel_data.data() + srcIndex;
                const uint8_t srcA = src[3];
                if (srcA == 0)
                    continue;

                const size_t dstIndex = static_cast<size_t>(py) * static_cast<size_t>(s_softrendererstate.width)
                    + static_cast<size_t>(px);

                const uint32_t dst = s_softrendererstate.framebuffer[dstIndex];

                const float alpha = static_cast<float>(srcA) / 255.0f;
                const float invAlpha = 1.0f - alpha;

                const uint8_t dstR = static_cast<uint8_t>((dst >> 16) & 0xFF);
                const uint8_t dstG = static_cast<uint8_t>((dst >> 8) & 0xFF);
                const uint8_t dstB = static_cast<uint8_t>(dst & 0xFF);
                const uint8_t dstA = static_cast<uint8_t>((dst >> 24) & 0xFF);

                const float outR = std::clamp(src[0] * alpha + dstR * invAlpha, 0.0f, 255.0f);
                const float outG = std::clamp(src[1] * alpha + dstG * invAlpha, 0.0f, 255.0f);
                const float outB = std::clamp(src[2] * alpha + dstB * invAlpha, 0.0f, 255.0f);
                const float dstAlpha = static_cast<float>(dstA) / 255.0f;
                const float outAlphaNorm = std::clamp(alpha + dstAlpha * invAlpha, 0.0f, 1.0f);
                const float outA = outAlphaNorm * 255.0f;

                const uint32_t packed = (static_cast<uint32_t>(outA + 0.5f) << 24)
                    | (static_cast<uint32_t>(outR + 0.5f) << 16)
                    | (static_cast<uint32_t>(outG + 0.5f) << 8)
                    | static_cast<uint32_t>(outB + 0.5f);

                s_softrendererstate.framebuffer[dstIndex] = packed;
            }
        }
    }

    // Main process loop
    inline bool softrenderer_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue)
    {
        auto& sr = s_softrendererstate;

        atlasmanager::process_pending_uploads(core::ContextType::Software);

        const size_t expectedSize =
            static_cast<size_t>((std::max)(1, sr.width)) * static_cast<size_t>((std::max)(1, sr.height));
        if (sr.framebuffer.size() != expectedSize) {
            resize_framebuffer(sr.width, sr.height);
        }

        // Clear framebuffer
        std::fill(sr.framebuffer.begin(), sr.framebuffer.end(), 0xFF000000);

        // Drain queued commands (your draw calls should write into sr.framebuffer)
        queue.drain();

#ifdef _WIN32
        // Present framebuffer to window
        HDC hdc = ctx ? ctx->hdc : nullptr;
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

    inline void softrenderer_cleanup(std::shared_ptr<core::Context> /*ctx*/)
    {
        auto& sr = s_softrendererstate;

        if (s_cleanupIssued.exchange(true, std::memory_order_acq_rel)) {
            return;
        }

        sr.framebuffer.clear();
        cubeTexture.reset();

#ifdef _WIN32
        // Only destroy if we created it (we didn't in this backend).
        if (s_createdOwnWindow && s_softrendererstate.hwnd) {
            DestroyWindow(s_softrendererstate.hwnd);
        }
        s_softrendererstate.hwnd = nullptr;
#endif

        s_softrendererstate = {};
        s_isDispatchingResize.store(false, std::memory_order_release);
        std::cout << "[SoftRenderer] Cleanup complete\n";
    }

    inline int get_width() { return s_softrendererstate.width; }
    inline int get_height() { return s_softrendererstate.height; }
}

#endif // ALMOND_USING_SOFTWARE_RENDERER
