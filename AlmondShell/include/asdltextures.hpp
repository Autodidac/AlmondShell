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
//asdltextures.hpp
#pragma once

#include "aplatform.hpp"
#include "aengineconfig.hpp"

#ifdef ALMOND_USING_SDL

#include "aatlasmanager.hpp"
#include "aatlastexture.hpp"
#include "aimageloader.hpp"
#include "atexture.hpp"
#include "asdlstate.hpp"

#include <atomic>
#include <filesystem>
#include <format>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <algorithm>

namespace almondnamespace::sdltextures
{
    using Handle = uint32_t;

    struct AtlasGPU {
        SDL_Texture* textureHandle = nullptr;
        u64 version = static_cast<u64>(-1); // force mismatch
        u32 width = 0;
        u32 height = 0;
    };

    inline std::atomic_uint8_t s_generation{ 1 };
    inline std::atomic_uint32_t s_dumpSerial{ 0 };

    inline SDL_Renderer* sdl_renderer = nullptr; // Must be set by your engine

    struct TextureAtlasPtrHash 
    {
        size_t operator()(const TextureAtlas* atlas) const noexcept {
            return std::hash<const TextureAtlas*>{}(atlas);
        }
    };

    struct TextureAtlasPtrEqual 
    {
        bool operator()(const TextureAtlas* lhs, const TextureAtlas* rhs) const noexcept {
            return lhs == rhs;
        }
    };
    inline std::unordered_map<const TextureAtlas*, AtlasGPU, TextureAtlasPtrHash, TextureAtlasPtrEqual> sdl_gpu_atlases;

    [[nodiscard]]
    inline Handle make_handle(int atlasIdx, int localIdx) noexcept 
    {
        return (Handle(s_generation.load(std::memory_order_relaxed)) << 24)
            | ((atlasIdx & 0xFFF) << 12)
            | (localIdx & 0xFFF);
    }

    [[nodiscard]]
    inline ImageData ensure_rgba(const ImageData& img) 
    {
        const size_t pixelCount = static_cast<size_t>(img.width) * img.height;
        const size_t channels = img.pixels.size() / pixelCount;

        if (channels == 4) return img;

        if (channels != 3)
            throw std::runtime_error("ensure_rgba(): Unsupported channel count: " + std::to_string(channels));

        std::vector<uint8_t> rgba(pixelCount * 4);
        const uint8_t* src = img.pixels.data();
        uint8_t* dst = rgba.data();

        for (size_t i = 0; i < pixelCount; ++i) {
            dst[4 * i + 0] = src[3 * i + 0];
            dst[4 * i + 1] = src[3 * i + 1];
            dst[4 * i + 2] = src[3 * i + 2];
            dst[4 * i + 3] = 255;
        }

        return { std::move(rgba), img.width, img.height, 4 };
    }

    inline std::string make_dump_name(int atlasIdx, std::string_view tag)
    {
        // Ensure `atlases/` folder exists
        std::filesystem::create_directories("atlases");
        // Format filename: atlases/<tag>_<atlasIdx>_<serial>.ppm
        return std::format("atlases/{}_{}_{}.ppm", tag, atlasIdx, s_dumpSerial.fetch_add(1, std::memory_order_relaxed));
    }

    inline void dump_atlas(const TextureAtlas& atlas, int atlasIdx)
    {
        const std::string filename = make_dump_name(atlasIdx, atlas.name);
        std::ofstream out(filename, std::ios::binary);
        if (!out) {
            std::cerr << "[Dump] Failed to open: " << filename << "\n";
            return;
        }

        // Write P6 header
        out << "P6\n" << atlas.width << " " << atlas.height << "\n255\n";

        // Dump RGB only (skip A)
        for (size_t i = 0; i < atlas.pixel_data.size(); i += 4) {
            out.put(static_cast<char>(atlas.pixel_data[i]));
            out.put(static_cast<char>(atlas.pixel_data[i + 1]));
            out.put(static_cast<char>(atlas.pixel_data[i + 2]));
        }

        std::cerr << "[Dump] Wrote: " << filename << "\n";
    }


    inline void upload_atlas_to_gpu(const TextureAtlas& atlas) 
    {
        if (!sdl_renderer)
            throw std::runtime_error("[SDL] Renderer not set!");


        if (atlas.pixel_data.empty()) {
            const_cast<TextureAtlas&>(atlas).rebuild_pixels();
        }

        auto& gpu = sdl_gpu_atlases[&atlas];

        if (gpu.version == atlas.version && gpu.textureHandle != nullptr) {
            std::cerr << "[UploadAtlas] SKIPPING for '" << atlas.name << "'\n";
            return;
        }

        if (gpu.textureHandle) {
            SDL_DestroyTexture(gpu.textureHandle);
            gpu.textureHandle = nullptr;
        }

        SDL_Surface* surface = SDL_CreateSurfaceFrom(
            atlas.width,                 // int width
            atlas.height,                // int height
            SDL_PIXELFORMAT_RGBA32,      // SDL_PixelFormat
            atlas.pixel_data.data(),     // void* pixels
            atlas.width * 4              // int pitch
        );

        if (!surface) throw std::runtime_error("[SDL] Failed: SDL_CreateSurfaceFrom");

        gpu.textureHandle = SDL_CreateTextureFromSurface(sdl_renderer, surface);

        SDL_DestroySurface(surface);

        if (!gpu.textureHandle)
            throw std::runtime_error("[SDL] Failed: SDL_CreateTextureFromSurface");

        gpu.width = atlas.width;
        gpu.height = atlas.height;
        gpu.version = atlas.version;

        dump_atlas(atlas, atlas.index);

        std::cerr << "[SDL] Uploaded atlas '" << atlas.name << "'\n";
    }

    inline void ensure_uploaded(const TextureAtlas& atlas) 
    {
        auto it = sdl_gpu_atlases.find(&atlas);
        if (it != sdl_gpu_atlases.end()) {
            if (it->second.version == atlas.version && it->second.textureHandle != nullptr)
                return;
        }
        upload_atlas_to_gpu(atlas);
    }

    inline Handle load_atlas(const TextureAtlas& atlas, int atlasIndex = -1) {
        atlasmanager::ensure_uploaded(atlas);
        const int resolvedIndex = (atlasIndex >= 0) ? atlasIndex : atlas.get_index();
        return make_handle(resolvedIndex, 0);
    }

    inline Handle atlas_add_texture(TextureAtlas& atlas, const std::string& id, const ImageData& img)
    {
        auto rgba = ensure_rgba(img);

        Texture texture{
            .width = static_cast<uint32_t>(rgba.width),
            .height = static_cast<uint32_t>(rgba.height),
            .pixels = std::move(rgba.pixels)
        };

        auto addedOpt = atlas.add_entry(id, texture);
        if (!addedOpt) {
            throw std::runtime_error("atlas_add_texture: Failed to add: " + id);
        }

        atlasmanager::ensure_uploaded(atlas);

        return make_handle(atlas.get_index(), addedOpt->index);
    }

    inline void clear_gpu_atlases() noexcept 
    {
        for (auto& [_, gpu] : sdl_gpu_atlases) {
            if (gpu.textureHandle) {
                SDL_DestroyTexture(gpu.textureHandle);
            }
        }
        sdl_gpu_atlases.clear();
        s_generation.fetch_add(1, std::memory_order_relaxed);
    }

    /// These come from context renderer 
    inline bool is_handle_live(const SpriteHandle& handle) noexcept {
        return handle.is_valid();
    }

    inline void draw_sprite(SpriteHandle handle, std::span<const TextureAtlas* const> atlases, float x, float y, float width, float height) noexcept
    {
        if (!is_handle_live(handle)) {
            std::cerr << "[SDL_DrawSprite] Invalid sprite handle.\n";
            return;
        }

        const int w = sdlcontext::state::s_sdlstate.window.width;
        const int h = sdlcontext::state::s_sdlstate.window.height;
        if (w == 0 || h == 0) {
            std::cerr << "[SDL_DrawSprite] ERROR: Window dimensions are zero.\n";
            return;
        }

        const int atlasIdx = int(handle.atlasIndex);
        const int localIdx = int(handle.localIndex);

        if (atlasIdx < 0 || atlasIdx >= int(atlases.size())) {
            std::cerr << "[SDL_DrawSprite] Atlas index out of bounds: " << atlasIdx << '\n';
            return;
        }

        const TextureAtlas* atlas = atlases[atlasIdx];
        if (!atlas) {
            std::cerr << "[SDL_DrawSprite] Null atlas pointer at index: " << atlasIdx << '\n';
            return;
        }
        AtlasRegion region{};
        if (!atlas->try_get_entry_info(localIdx, region)) {
            std::cerr << "[SDL_DrawSprite] Sprite index out of bounds: " << localIdx << '\n';
            return;
        }

        //std::cerr << "[SDL_DrawSprite] Using atlas: '" << atlas->name << "', sprite: '" << atlas->entries[localIdx].name << "'\n";

        ensure_uploaded(*atlas); // Your SDL texture upload variant

        auto it = sdl_gpu_atlases.find(atlas);
        if (it == sdl_gpu_atlases.end()) {
            std::cerr << "[SDL_DrawSprite] GPU texture not found for atlas '" << atlas->name << "'\n";
            return;
        }

        SDL_Texture* texture = it->second.textureHandle;
        if (!texture) {
            std::cerr << "[SDL_DrawSprite] GPU texture handle is null for atlas '" << atlas->name << "'\n";
            return;
        }

        int outputW = w;
        int outputH = h;
        if (sdl_renderer) {
            SDL_GetCurrentRenderOutputSize(sdl_renderer, &outputW, &outputH);
        }

        SDL_Rect viewport{ 0, 0, outputW, outputH };
        if (sdl_renderer) {
            SDL_Rect currentViewport{ 0, 0, 0, 0 };
            if (SDL_GetRenderViewport(sdl_renderer, &currentViewport) == 0 &&
                currentViewport.w > 0 && currentViewport.h > 0) {
                viewport = currentViewport;
            }
        }

        float drawX = static_cast<float>(x);
        float drawY = static_cast<float>(y);
        float drawWidth = static_cast<float>(width);
        float drawHeight = static_cast<float>(height);

        const bool widthNormalized = drawWidth > 0.f && drawWidth <= 1.f;
        const bool heightNormalized = drawHeight > 0.f && drawHeight <= 1.f;
        const bool xNormalized = drawX >= 0.f && drawX <= 1.f;
        const bool yNormalized = drawY >= 0.f && drawY <= 1.f;

        const float baseWidth = static_cast<float>((std::max)(1, viewport.w));
        const float baseHeight = static_cast<float>((std::max)(1, viewport.h));

        if (widthNormalized) {
            drawWidth = (std::max)(drawWidth * baseWidth, 1.0f);
        }
        if (heightNormalized) {
            drawHeight = (std::max)(drawHeight * baseHeight, 1.0f);
        }

        if (xNormalized) {
            drawX = drawX * baseWidth;
        }
        if (yNormalized) {
            drawY = drawY * baseHeight;
        }

        if (drawWidth <= 0.f)
            drawWidth = static_cast<float>(region.width);
        if (drawHeight <= 0.f)
            drawHeight = static_cast<float>(region.height);

        drawX += static_cast<float>(viewport.x);
        drawY += static_cast<float>(viewport.y);

        SDL_FRect dstRect{
            drawX,
            drawY,
            drawWidth,
            drawHeight
        };

        SDL_FRect srcRect{
            static_cast<float>(region.x),
            static_cast<float>(region.y),
            static_cast<float>(region.width),
            static_cast<float>(region.height)
        };


        //std::cout << "[DEBUG] draw_sprite: "
        //    << "x=" << x << ", y=" << y
        //    << ", width=" << width << ", height=" << height << "\n";

        //std::cout << "Atlas size: " << atlas->width << "x" << atlas->height << "\n";
        //std::cout << "Region: x=" << entry.region.x << " y=" << entry.region.y
        //    << " w=" << entry.region.width << " h=" << entry.region.height << "\n";
        //std::cout << "Dest: x=" << dstRect.x << " y=" << dstRect.y
        //    << " w=" << dstRect.w << " h=" << dstRect.h << "\n";

        SDL_RenderTexture(sdl_renderer, texture, &srcRect, &dstRect);

        sdlcontext::check_sdl_error("SDL_RenderTexture");
    }
} // namespace almondnamespace::sdl

#endif // ALMOND_USING_SDL
