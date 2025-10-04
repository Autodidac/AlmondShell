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
// aopengltextures.hpp
#pragma once

#include "aplatform.hpp"
#include "aengineconfig.hpp"
#include "atypes.hpp"

#ifdef ALMOND_USING_OPENGL

#include "acontext.hpp"
//#include "acontextmultiplexer.hpp"  // declares g_backends
#include "aopenglstate.hpp"
#include "aatlasmanager.hpp"
#include "aatlastexture.hpp"
#include "aimageloader.hpp"
#include "atexture.hpp"
#include "aspritehandle.hpp"
#include "acommandline.hpp"

#include <atomic>
#include <format>
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <mutex>
#include <algorithm>

namespace almondnamespace::opengltextures
{
    struct AtlasGPU 
    {
        GLuint textureHandle = 0;
        u64 version = static_cast<u64>(-1);  // force mismatch on first compare
        u32 width = 0;
        u32 height = 0;
    };

    struct TextureAtlasPtrHash {
        size_t operator()(const TextureAtlas* atlas) const noexcept {
            return std::hash<const TextureAtlas*>{}(atlas);
        }
    };

    struct TextureAtlasPtrEqual {
        bool operator()(const TextureAtlas* lhs, const TextureAtlas* rhs) const noexcept {
            return lhs == rhs;
        }
    };

    inline std::unordered_map<const TextureAtlas*, AtlasGPU, TextureAtlasPtrHash, TextureAtlasPtrEqual> opengl_gpu_atlases;

    // forward declare OpenGL4State to avoid pulling in aopenglcontext here
    namespace openglcontext { struct OpenGL4State; }
    struct BackendData {
        std::unordered_map<const TextureAtlas*, AtlasGPU,
            TextureAtlasPtrHash, TextureAtlasPtrEqual> gpu_atlases;
        std::mutex gpuMutex;
        almondnamespace::openglcontext::OpenGL4State glState{};
    };

    inline BackendData& get_opengl_backend() {
        BackendData* data = nullptr;
        {
            std::unique_lock lock(almondnamespace::core::g_backendsMutex);
            auto& backend = almondnamespace::core::g_backends[almondnamespace::core::ContextType::OpenGL];
            if (!backend.data) {
                backend.data = {
                    new BackendData(),
                    [](void* p) { delete static_cast<BackendData*>(p); }
                };
            }
            data = static_cast<BackendData*>(backend.data.get());
        }
        return *data;
    }

   // using almondnamespace::openglcontext::s_openglstate;
    using Handle = uint32_t;

    inline std::atomic_uint8_t s_generation{ 0 };
    inline std::atomic_uint32_t s_dumpSerial{ 0 };

    [[nodiscard]]
    inline Handle make_handle(int atlasIdx, int localIdx) noexcept {
        return (Handle(s_generation.load(std::memory_order_relaxed)) << 24)
            | ((atlasIdx & 0xFFF) << 12)
            | (localIdx & 0xFFF);
    }

    [[nodiscard]]
    inline bool is_handle_live(Handle h) noexcept {
        return uint8_t(h >> 24) == s_generation.load(std::memory_order_relaxed);
    }

    [[nodiscard]]
    inline ImageData ensure_rgba(const ImageData& img) {
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

    inline std::string make_dump_name(int atlasIdx, std::string_view tag) {
        std::filesystem::create_directories("atlas_dump");
        return std::format("atlas_dump/{}_{}_{}.ppm", tag, atlasIdx, s_dumpSerial.fetch_add(1, std::memory_order_relaxed));
    }

    inline void dump_atlas(const TextureAtlas& atlas, int atlasIdx) {
        std::string filename = make_dump_name(atlasIdx, atlas.name);
        std::ofstream out(filename, std::ios::binary);
        out << "P6\n" << atlas.width << " " << atlas.height << "\n255\n";
        for (size_t i = 0; i < atlas.pixel_data.size(); i += 4) {
            out.put(atlas.pixel_data[i]);
            out.put(atlas.pixel_data[i + 1]);
            out.put(atlas.pixel_data[i + 2]);
        }
        std::cerr << "[Dump] Wrote: " << filename << "\n";
    }

    inline void upload_atlas_to_gpu(const TextureAtlas& atlas)
    {
        BackendData* oglData = nullptr;
        {
            std::shared_lock lock(core::g_backendsMutex);
            auto it = core::g_backends.find(core::ContextType::OpenGL);
            if (it != core::g_backends.end()) {
                oglData = static_cast<BackendData*>(it->second.data.get());
            }
        }
        if (!oglData) {
            std::cerr << "[UploadAtlas] OpenGL backendData not initialized!\n";
            return;
        }
        auto& glState = oglData->glState;

        if (atlas.pixel_data.empty()) {
            std::cerr << "[UploadAtlas] Pixel data empty for '" << atlas.name
                << "', rebuilding...\n";
            const_cast<TextureAtlas&>(atlas).rebuild_pixels();
        }

        // Ensure we have a current GL context
        HGLRC oldCtx = wglGetCurrentContext();
        if (!oldCtx) {
            if (!glState.hdc || !glState.hglrc) {
                std::cerr << "[UploadAtlas] No current GL context and no state to make one!\n";
                return;
            }
            if (!wglMakeCurrent(glState.hdc, glState.hglrc)) {
                std::cerr << "[UploadAtlas] Failed to make GL context current for upload\n";
                return;
            }
        }

        std::lock_guard<std::mutex> gpuLock(oglData->gpuMutex);
        auto& gpu = oglData->gpu_atlases[&atlas];

        if (!gpu.textureHandle) {
            glGenTextures(1, &gpu.textureHandle);
            if (!gpu.textureHandle) {
                std::cerr << "[OpenGL] Failed to generate texture for atlas: "
                    << atlas.name << "\n";
                return;
            }
        }

        if (gpu.version == atlas.version) {
            std::cerr << "[UploadAtlas] SKIPPING upload for '" << atlas.name
                << "' version = " << atlas.version << "\n";
            return;
        }

        glBindTexture(GL_TEXTURE_2D, gpu.textureHandle);

        if (gpu.width != atlas.width || gpu.height != atlas.height) {
#ifdef GL_ARB_texture_storage
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, atlas.width, atlas.height);
#else
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                atlas.width, atlas.height,
                0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
#endif
            gpu.width = atlas.width;
            gpu.height = atlas.height;
        }

        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
            atlas.width, atlas.height,
            GL_RGBA, GL_UNSIGNED_BYTE,
            atlas.pixel_data.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        gpu.version = atlas.version;

        glBindTexture(GL_TEXTURE_2D, 0);

        std::cerr << "[OpenGL] Uploaded atlas '" << atlas.name
            << "' (tex id " << gpu.textureHandle << ")\n";

        if (!oldCtx) {
            wglMakeCurrent(nullptr, nullptr);
        }
    }

    inline void ensure_uploaded(const TextureAtlas& atlas)
    {
        BackendData* oglData = nullptr;
        {
            std::shared_lock lock(core::g_backendsMutex);
            auto it = core::g_backends.find(core::ContextType::OpenGL);
            if (it != core::g_backends.end()) {
                oglData = static_cast<BackendData*>(it->second.data.get());
            }
        }
        if (!oglData) {
            std::cerr << "[EnsureUploaded] OpenGL backendData not initialized!\n";
            return;
        }

        {
            std::lock_guard<std::mutex> gpuLock(oglData->gpuMutex);
            auto it = oglData->gpu_atlases.find(&atlas);
            if (it != oglData->gpu_atlases.end()) {
                if (it->second.version == atlas.version && it->second.textureHandle != 0)
                    return;
            }
        }
        upload_atlas_to_gpu(atlas);
    }



    inline void clear_gpu_atlases() noexcept {
        for (auto& [_, gpu] : opengl_gpu_atlases) {
            if (gpu.textureHandle) {
                glDeleteTextures(1, &gpu.textureHandle);
            }
        }
        opengl_gpu_atlases.clear();
        s_generation.fetch_add(1, std::memory_order_relaxed);
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
            throw std::runtime_error("atlas_add_texture: Failed to add texture: " + id);
        }

        atlasmanager::ensure_uploaded(atlas);

        return make_handle(atlas.get_index(), addedOpt->index);
    }

    inline uint32_t upload_texture(const uint8_t* pixels, int width, int height) 
    {
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

        glBindTexture(GL_TEXTURE_2D, 0);

        return tex;
    }

    // Diagnostic draw_sprite version
    inline void draw_sprite(SpriteHandle handle,
        std::span<const TextureAtlas* const> atlases,
        float x, float y, float width, float height) noexcept
    {
        if (!handle.is_valid()) {
            std::cerr << "[DrawSprite] Invalid sprite handle.\n";
            return;
        }

        const int w = core::cli::window_width;
        const int h = core::cli::window_height;
        if (w == 0 || h == 0) {
            std::cerr << "[DrawSprite] ERROR: Window dimensions are zero.\n";
            return;
        }

#if defined(DEBUG_TEXTURE_RENDERING)
        std::cerr << "[DrawSprite] Inputs: x=" << x
            << ", y=" << y << ", w=" << width
            << ", h=" << height << '\n';
#endif

        const int atlasIdx = int(handle.atlasIndex);
        const int localIdx = int(handle.localIndex);

        if (atlasIdx < 0 || atlasIdx >= int(atlases.size())) {
            std::cerr << "[DrawSprite] Atlas index out of bounds: " << atlasIdx << '\n';
            return;
        }
        const TextureAtlas* atlas = atlases[atlasIdx];
        if (!atlas) {
            std::cerr << "[DrawSprite] Null atlas pointer at index: " << atlasIdx << '\n';
            return;
        }
        AtlasRegion region{};
        std::string spriteName;
        if (!atlas->try_get_entry_info(localIdx, region, &spriteName)) {
            std::cerr << "[DrawSprite] Sprite index out of bounds: " << localIdx << '\n';
            return;
        }

        ensure_uploaded(*atlas);

        // 🔑 FIX: use backend.gpu_atlases, not global opengl_gpu_atlases
        auto& backend = get_opengl_backend();
        GLuint tex = 0;
        {
            std::lock_guard<std::mutex> gpuLock(backend.gpuMutex);
            auto it = backend.gpu_atlases.find(atlas);
            if (it == backend.gpu_atlases.end()) {
                std::cerr << "[DrawSprite] GPU texture not found for atlas '"
                    << atlas->name << "'\n";
                return;
            }
            tex = it->second.textureHandle;
        }
        if (!tex) {
            std::cerr << "[DrawSprite] GPU texture not found for atlas '"
                << atlas->name << "'\n";
            return;
        }

        glUseProgram(backend.glState.shader);
        glBindVertexArray(backend.glState.vao);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);

        // disable mipmapping for pixel perfect rendering
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        const bool widthNormalized = width > 0.f && width <= 1.f;
        const bool heightNormalized = height > 0.f && height <= 1.f;

        float drawWidth = widthNormalized ? std::max(width * float(w), 1.0f) : width;
        float drawHeight = heightNormalized ? std::max(height * float(h), 1.0f) : height;

        float drawX = (widthNormalized && x >= 0.f && x <= 1.f)
            ? x * float(w)
            : x;
        float drawY = (heightNormalized && y >= 0.f && y <= 1.f)
            ? y * float(h)
            : y;

        const float u0 = region.u1;
        const float du = region.u2 - region.u1;
        const float v0 = region.v1;
        const float dv = region.v2 - region.v1;

        if (backend.glState.uUVRegionLoc >= 0)
            glUniform4f(backend.glState.uUVRegionLoc, u0, v0, du, dv);

        // Flip Y pixel coordinate *before* normalization
        float flippedY = h - (drawY + drawHeight * 0.5f);

        // Convert to NDC [-1, 1], center coords
        float ndc_x = ((drawX + drawWidth * 0.5f) / float(w)) * 2.f - 1.f;
        float ndc_y = (flippedY / float(h)) * 2.f - 1.f;
        float ndc_w = (drawWidth / float(w)) * 2.f;
        float ndc_h = (drawHeight / float(h)) * 2.f;

#if defined(DEBUG_TEXTURE_RENDERING_VERY_VERBOSE)
        std::cerr << "[DrawSprite] Atlas entries count: " << atlas->entry_count() << "\n";
        std::cerr << "[DrawSprite] Requested sprite index: " << localIdx << "\n";

        if (y < 0) {
            std::cerr << "[Warning] Negative y coordinate: " << y << '\n';
        }

        std::cerr << "[DrawSprite] Using region for '" << spriteName << "': "
            << "u1=" << region.u1 << ", v1=" << region.v1
            << ", u2=" << region.u2 << ", v2=" << region.v2
            << ", x=" << region.x << ", y=" << region.y
            << ", w=" << region.width << ", h=" << region.height << '\n';
#endif

        if (backend.glState.uTransformLoc >= 0)
            glUniform4f(backend.glState.uTransformLoc, ndc_x, ndc_y, ndc_w, ndc_h);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cerr << "[OpenGL ERROR] glDrawElements failed: " << std::hex << err << "\n";
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_BLEND);

#if defined(DEBUG_TEXTURE_RENDERING_VERY_VERBOSE)
        std::cerr << "[DrawSprite] Atlas '" << atlas->name
            << "' Sprite '" << spriteName
            << "' AtlasIdx=" << atlasIdx
            << " SpriteIdx=" << localIdx << '\n';

        std::cerr << "[DrawSprite] Final quad NDC: X=" << ndc_x
            << " Y=" << ndc_y
            << " W=" << ndc_w
            << " H=" << ndc_h << "\n";
#endif
    }


} // namespace almondnamespace::opengl

#endif // ALMOND_USING_OPENGL
