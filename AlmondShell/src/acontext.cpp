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
 // acontext.cpp
#include "pch.h"

#include "aplatform.hpp"
#include "aengineconfig.hpp"

#include "acontext.hpp"
#include "acontextwindow.hpp"
#include "acontextmultiplexer.hpp"
#include "adiagnostics.hpp"

#ifdef ALMOND_USING_OPENGL
#include "aopenglcontext.hpp"
#include "aopenglrenderer.hpp"
#include "aopengltextures.hpp"
#endif
#ifdef ALMOND_USING_VULKAN
#include "avulkancontext.hpp"
#include "avulkanrenderer.hpp"
#include "avulkantextures.hpp"
#endif
#ifdef ALMOND_USING_DIRECTX
#include "adirectxcontext.hpp"
#include "adirectxrenderer.hpp"
#include "adirectxtextures.hpp"
#endif
#ifdef ALMOND_USING_SDL
#include "asdlcontext.hpp"
#include "asdlcontextrenderer.hpp"
#include "asdltextures.hpp"
#endif
#ifdef ALMOND_USING_SFML
#include "asfmlcontext.hpp"
#include "asfmlrenderer.hpp"
#include "asfmltextures.hpp"
#endif
#ifdef ALMOND_USING_RAYLIB
#include "araylibcontext.hpp"
#include "araylibrenderer.hpp"
#include "araylibtextures.hpp"
#endif
#ifdef ALMOND_USING_CUSTOM
#include "acustomcontext.hpp"
#include "acustomrenderer.hpp"
#include "acustomtextures.hpp"
#endif
#ifdef ALMOND_USING_SOFTWARE_RENDERER
#include "asoftrenderer_context.hpp"
#include "asoftrenderer_renderer.hpp"
#include "asoftrenderer_textures.hpp"
#endif

#include "aimageloader.hpp"
#include "aatlastexture.hpp"
#include "aatlasmanager.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>

namespace
{
#if defined(_WIN32)
    void ClampMouseToClientRectIfNeeded(const std::shared_ptr<almondnamespace::core::Context>& ctx, int& x, int& y) noexcept
    {
        if (!ctx || !ctx->hwnd) return;

        if (almondnamespace::input::are_mouse_coords_global()) {
            return; // Safe wrapper will translate + clamp when coordinates are global.
        }

        RECT rc{};
        if (!::GetClientRect(ctx->hwnd, &rc)) {
            return;
        }

        const int width = static_cast<int>((std::max)(static_cast<LONG>(1), rc.right - rc.left));
        const int height = static_cast<int>((std::max)(static_cast<LONG>(1), rc.bottom - rc.top));

        if (x < 0 || y < 0 || x >= width || y >= height) {
            x = -1;
            y = -1;
        }
    }
#elif defined(__linux__)
    void ClampMouseToClientRectIfNeeded(const std::shared_ptr<almondnamespace::core::Context>& ctx, int& x, int& y) noexcept
    {
        if (!ctx)
        {
            return;
        }

        if (almondnamespace::input::are_mouse_coords_global())
        {
            return;
        }

        const int width = (std::max)(1, ctx->width);
        const int height = (std::max)(1, ctx->height);

        if (x < 0 || y < 0 || x >= width || y >= height)
        {
            x = -1;
            y = -1;
        }
    }
#else
    void ClampMouseToClientRectIfNeeded(const std::shared_ptr<almondnamespace::core::Context>&, int&, int&) noexcept {}
#endif
}

namespace almondnamespace::core {

    // ─── Global backends ──────────────────────────────────────
    std::map<ContextType, BackendState> g_backends{};
    std::shared_mutex g_backendsMutex{};

    // ─── AddContextForBackend ─────────────────────────────────
    void AddContextForBackend(ContextType type, std::shared_ptr<Context> context) {
        std::unique_lock lock(g_backendsMutex);
        auto& backendState = g_backends[type];

        if (!backendState.master)
            backendState.master = std::move(context);
        else
            backendState.duplicates.emplace_back(std::move(context));

        // allocate backend-specific data if not already set
        if (!backendState.data) {
            switch (type) {
#ifdef ALMOND_USING_OPENGL
            case ContextType::OpenGL:
                backendState.data = {
                    new opengltextures::BackendData(),
                    [](void* p) { delete static_cast<opengltextures::BackendData*>(p); }
                };
                break;
#endif
#ifdef ALMOND_USING_SOFTWARE_RENDERER
            case ContextType::Software:
                backendState.data = {
                    new anativecontext::BackendData(),
                    [](void* p) { delete static_cast<anativecontext::BackendData*>(p); }
                };
                break;
#endif
            default:
                break;
            }
        }
    }

    // ─── Context::process_safe ────────────────────────────────
    bool Context::process_safe(std::shared_ptr<core::Context> ctx, CommandQueue& queue) {
        if (!process) return false;
        try {
            if (ctx)
            {
                almond::diagnostics::ContextDimensionSnapshot snapshot{};
                snapshot.contextId = ctx.get();
                snapshot.type = ctx->type;
                snapshot.backendName = ctx->backendName;
                snapshot.logicalWidth = ctx->width;
                snapshot.logicalHeight = ctx->height;
                snapshot.framebufferWidth = ctx->framebufferWidth;
                snapshot.framebufferHeight = ctx->framebufferHeight;
                snapshot.virtualWidth = ctx->virtualWidth;
                snapshot.virtualHeight = ctx->virtualHeight;
                almond::diagnostics::log_context_dimensions_if_changed(snapshot);
            }
            return process(ctx, queue);
        }
        catch (const std::exception& e) {
            std::cerr << "[Context] Exception in process: " << e.what() << "\n";
            return false;
        }
        catch (...) {
            std::cerr << "[Context] Unknown exception in process\n";
            return false;
        }
    }

    // ─── Atlas Thunks ─────────────────────────────────────────
    inline uint32_t AddTextureThunk(TextureAtlas& atlas, std::string name, const ImageData& img, ContextType type) {
        switch (type) {
#ifdef ALMOND_USING_OPENGL
        case ContextType::OpenGL: return opengltextures::atlas_add_texture(atlas, name, img);
#endif
#ifdef ALMOND_USING_SDL
        case ContextType::SDL:  return sdltextures::atlas_add_texture(atlas, name, img);
#endif
#ifdef ALMOND_USING_SFML
        case ContextType::SFML:  return sfmltextures::atlas_add_texture(atlas, name, img);
#endif
#ifdef ALMOND_USING_RAYLIB
        case ContextType::RayLib: return raylibtextures::atlas_add_texture(atlas, name, img);
#endif
#ifdef ALMOND_USING_VULKAN
        case ContextType::Vulkan: return vulkantextures::atlas_add_texture(atlas, name, img);
#endif
#ifdef ALMOND_USING_DIRECTX
        case ContextType::DirectX: return directxtextures::atlas_add_texture(atlas, name, img);
#endif
        default: (void)atlas; (void)name; (void)img; return 0;
        }
    }

    inline uint32_t AddAtlasThunk(const TextureAtlas& atlas, ContextType type) {
        atlasmanager::ensure_uploaded(atlas);

        uint32_t handle = 0;
        switch (type) {
#ifdef ALMOND_USING_OPENGL
        case ContextType::OpenGL: handle = opengltextures::load_atlas(atlas, atlas.get_index()); break;
#endif
#ifdef ALMOND_USING_SDL
        case ContextType::SDL:  handle = sdltextures::load_atlas(atlas, atlas.get_index()); break;
#endif
#ifdef ALMOND_USING_SFML
        case ContextType::SFML: handle = sfmltextures::load_atlas(atlas, atlas.get_index()); break;
#endif
#ifdef ALMOND_USING_RAYLIB
        case ContextType::RayLib: handle = raylibtextures::load_atlas(atlas, atlas.get_index()); break;
#endif
#ifdef ALMOND_USING_VULKAN
        case ContextType::Vulkan: handle = vulkantextures::load_atlas(atlas, atlas.get_index()); break;
#endif
#ifdef ALMOND_USING_DIRECTX
        case ContextType::DirectX: handle = directxtextures::load_atlas(atlas, atlas.get_index()); break;
#endif
        case ContextType::Software:
        case ContextType::Custom:
        case ContextType::None:
        case ContextType::Noop: {
            // CPU-only or placeholder contexts do not upload to a backend; return a
            // stable, non-zero identifier derived from the atlas index so callers
            // can treat the operation as successful.
            const int atlasIndex = atlas.get_index();
            handle = static_cast<uint32_t>(atlasIndex >= 0 ? atlasIndex + 1 : 1);
            break;
        }
        default:
            std::cerr << "[AddAtlasThunk] Unsupported context type\n";
            break;
        }
        if (handle != 0) {
            atlasmanager::process_pending_uploads(type);
        }
        return handle;
    }

    // ─── Backend stubs (minimal no-op defaults) ──────────────
#ifdef ALMOND_USING_OPENGL
    inline void opengl_initialize() {}
    inline void opengl_cleanup() {}
    bool opengl_process(std::shared_ptr<core::Context> ctx, CommandQueue& queue) {
        atlasmanager::process_pending_uploads(ctx->type);
        queue.drain();
        return true;
    }
    inline void opengl_clear() {}
    inline void opengl_present() {}
    inline int  opengl_get_width() { return 800; }
    inline int  opengl_get_height() { return 600; }
#endif

#ifdef ALMOND_USING_SDL
    inline void sdl_initialize() {}
    inline void sdl_cleanup() {}
    bool sdl_process(std::shared_ptr<core::Context> ctx, CommandQueue& queue) {
        if (!ctx) return false;
        atlasmanager::process_pending_uploads(ctx->type);
        queue.drain();
        return true;
    }
    inline void sdl_clear() {}
    inline void sdl_present() {}
    inline int  sdl_get_width() { return 800; }
    inline int  sdl_get_height() { return 600; }
#endif

#ifdef ALMOND_USING_SFML
    inline void sfml_initialize() {}
    inline void sfml_cleanup() {}
    bool sfml_process(std::shared_ptr<core::Context> ctx, CommandQueue& queue) {
        if (!ctx) return false;
        atlasmanager::process_pending_uploads(ctx->type);
        queue.drain();
        return true;
    }
    inline void sfml_clear() {}
    inline void sfml_present() {}
    inline int  sfml_get_width() { return 800; }
    inline int  sfml_get_height() { return 600; }
#endif

#ifdef ALMOND_USING_RAYLIB
    inline void raylib_initialize() {}
    inline void raylib_cleanup() {}
    bool raylib_process(std::shared_ptr<core::Context> ctx, CommandQueue& queue) {
        if (!ctx) return false;
        atlasmanager::process_pending_uploads(ctx->type);
        queue.drain();
        return true;
    }
    inline void raylib_clear() {}
    inline void raylib_present() {}
    inline int  raylib_get_width() { return 800; }
    inline int  raylib_get_height() { return 600; }
#endif

#ifdef ALMOND_USING_VULKAN
    inline void vulkan_initialize() {}
    inline void vulkan_cleanup() {}
    bool vulkan_process(std::shared_ptr<core::Context> ctx, CommandQueue& queue) {
        if (!ctx) return false;
        atlasmanager::process_pending_uploads(ctx->type);
        queue.drain();
        return true;
    }
    inline void vulkan_clear() {}
    inline void vulkan_present() {}
    inline int  vulkan_get_width() { return 800; }
    inline int  vulkan_get_height() { return 600; }
#endif

#ifdef ALMOND_USING_DIRECTX
    inline void directx_initialize() {}
    inline void directx_cleanup() {}
    bool directx_process(std::shared_ptr<core::Context> ctx, CommandQueue& queue) {
        if (!ctx) return false;
        atlasmanager::process_pending_uploads(ctx->type);
        queue.drain();
        return true;
    }
    inline void directx_clear() {}
    inline void directx_present() {}
    inline int  directx_get_width() { return 800; }
    inline int  directx_get_height() { return 600; }
#endif

#ifdef ALMOND_USING_CUSTOM
    inline void custom_initialize() {}
    inline void custom_cleanup() {}
    bool custom_process(std::shared_ptr<core::Context> ctx, CommandQueue& queue) {
        if (!ctx) return false;
        atlasmanager::process_pending_uploads(ctx->type);
        queue.drain();
        return true;
    }
    inline void custom_clear() {}
    inline void custom_present() {}
    inline int  custom_get_width() { return 800; }
    inline int  custom_get_height() { return 600; }
#endif

#ifdef ALMOND_USING_SOFTWARE_RENDERER
    inline void softrenderer_initialize() {}
    inline void softrenderer_cleanup() {}
    bool softrenderer_process(std::shared_ptr<core::Context> ctx, CommandQueue& queue) {
        if (!ctx) return false;
        atlasmanager::process_pending_uploads(ctx->type);
        queue.drain();
        return true;
    }
    inline void softrenderer_clear() {}
    inline void softrenderer_present() {}
    inline int  softrenderer_get_width() { return 800; }
    inline int  softrenderer_get_height() { return 600; }
#endif


    // ─── Init all backends ───────────────────────────────────
    namespace {
        inline void copy_atomic_function(AlmondAtomicFunction<uint32_t(TextureAtlas&, std::string, const ImageData&)>& dst,
            const AlmondAtomicFunction<uint32_t(TextureAtlas&, std::string, const ImageData&)>& src) {
            dst.ptr.store(src.ptr.load(std::memory_order_acquire), std::memory_order_release);
        }

        inline void copy_atomic_function(AlmondAtomicFunction<uint32_t(const TextureAtlas&)>& dst,
            const AlmondAtomicFunction<uint32_t(const TextureAtlas&)>& src) {
            dst.ptr.store(src.ptr.load(std::memory_order_acquire), std::memory_order_release);
        }

#ifdef ALMOND_USING_OPENGL
        void opengl_clear_adapter() {
            if (auto ctx = MultiContextManager::GetCurrent()) {
                almondnamespace::openglcontext::opengl_clear(ctx);
            }
        }

        void opengl_cleanup_adapter() {
            if (auto ctx = MultiContextManager::GetCurrent()) {
                auto copy = ctx;
                almondnamespace::openglcontext::opengl_cleanup(copy);
            }
        }
#endif

#ifdef ALMOND_USING_SOFTWARE_RENDERER
        void softrenderer_cleanup_adapter() {
            if (auto ctx = MultiContextManager::GetCurrent()) {
                auto copy = ctx;
                almondnamespace::anativecontext::softrenderer_cleanup(copy);
            }
        }
#endif

#ifdef ALMOND_USING_SDL
        void sdl_cleanup_adapter() {
            if (auto ctx = MultiContextManager::GetCurrent()) {
                auto copy = ctx;
                almondnamespace::sdlcontext::sdl_cleanup(copy);
            }
        }
#endif

#ifdef ALMOND_USING_SFML
        void sfml_cleanup_adapter() {
            if (auto ctx = MultiContextManager::GetCurrent()) {
                auto copy = ctx;
                almondnamespace::sfmlcontext::sfml_cleanup(copy);
            }
        }
#endif

#ifdef ALMOND_USING_RAYLIB
        void raylib_cleanup_adapter() {
            if (auto ctx = MultiContextManager::GetCurrent()) {
                auto copy = ctx;
                almondnamespace::raylibcontext::raylib_cleanup(copy);
            }
        }
#endif
    }


    std::shared_ptr<Context> CloneContext(const Context& prototype) {
        auto clone = std::make_shared<Context>();

        clone->initialize = prototype.initialize;
        clone->cleanup = prototype.cleanup;
        clone->process = prototype.process;
        clone->clear = prototype.clear;
        clone->present = prototype.present;
        clone->get_width = prototype.get_width;
        clone->get_height = prototype.get_height;
        clone->registry_get = prototype.registry_get;
        clone->draw_sprite = prototype.draw_sprite;
        clone->add_model = prototype.add_model;

        clone->is_key_held = prototype.is_key_held;
        clone->is_key_down = prototype.is_key_down;
        clone->get_mouse_position = prototype.get_mouse_position;
        clone->is_mouse_button_held = prototype.is_mouse_button_held;
        clone->is_mouse_button_down = prototype.is_mouse_button_down;

        copy_atomic_function(clone->add_texture, prototype.add_texture);
        copy_atomic_function(clone->add_atlas, prototype.add_atlas);

        clone->onResize = prototype.onResize;

        clone->width = prototype.width;
        clone->height = prototype.height;
        clone->type = prototype.type;
        clone->backendName = prototype.backendName;

        // Window specific handles/state must be assigned later by the caller
        clone->hwnd = nullptr;
        clone->hdc = nullptr;
        clone->hglrc = nullptr;
        clone->windowData = nullptr;

        return clone;
    }

    void InitializeAllContexts() {
        static bool s_initialized = false;
        if (s_initialized) return;
        s_initialized = true;

#if defined(ALMOND_USING_OPENGL)
        auto openglContext = std::make_shared<Context>();
        openglContext->initialize = opengl_initialize;
        openglContext->cleanup = opengl_cleanup_adapter;
        openglContext->process = almondnamespace::openglcontext::opengl_process;
        openglContext->clear = opengl_clear_adapter;
        openglContext->present = almondnamespace::openglcontext::opengl_present;
        openglContext->get_width = almondnamespace::openglcontext::opengl_get_width;
        openglContext->get_height = almondnamespace::openglcontext::opengl_get_height;

        openglContext->is_key_held = [](input::Key k) { return input::is_key_held(k); };
        openglContext->is_key_down = [](input::Key k) { return input::is_key_down(k); };
        openglContext->get_mouse_position = [weakCtx = std::weak_ptr<Context>(openglContext)](int& x, int& y) {
            x = input::mouseX.load(std::memory_order_relaxed);
            y = input::mouseY.load(std::memory_order_relaxed);
            if (auto ctx = weakCtx.lock()) {
                ClampMouseToClientRectIfNeeded(ctx, x, y);
            }
        };
        openglContext->is_mouse_button_held = [](input::MouseButton b) { return input::is_mouse_button_held(b); };
        openglContext->is_mouse_button_down = [](input::MouseButton b) { return input::is_mouse_button_down(b); };

        openglContext->registry_get = [](const char*) { return 0; };
        openglContext->draw_sprite = opengltextures::draw_sprite;

        openglContext->add_texture = [&](TextureAtlas& a, const std::string& n, const ImageData& i) {
            return AddTextureThunk(a, n, i, ContextType::OpenGL);
            };
        openglContext->add_atlas = [&](const TextureAtlas& a) {
            return AddAtlasThunk(a, ContextType::OpenGL);
            };

        openglContext->add_model = [](const char*, const char*) { return 0; };

        openglContext->backendName = "OpenGL";
        openglContext->type = ContextType::OpenGL;
        AddContextForBackend(ContextType::OpenGL, openglContext);
        atlasmanager::register_backend_uploader(ContextType::OpenGL,
            [](const TextureAtlas& atlas) {
                opengltextures::ensure_uploaded(atlas);
            });
#endif

#if defined(ALMOND_USING_SDL)
        auto sdlContext = std::make_shared<Context>();
        sdlContext->initialize = sdl_initialize;
        sdlContext->cleanup = sdl_cleanup_adapter;
        sdlContext->process = almondnamespace::sdlcontext::sdl_process;
        sdlContext->clear = almondnamespace::sdlcontext::sdl_clear;
        sdlContext->present = almondnamespace::sdlcontext::sdl_present;
        sdlContext->get_width = almondnamespace::sdlcontext::sdl_get_width;
        sdlContext->get_height = almondnamespace::sdlcontext::sdl_get_height;

        sdlContext->is_key_held = [](input::Key k) { return input::is_key_held(k); };
        sdlContext->is_key_down = [](input::Key k) { return input::is_key_down(k); };
        sdlContext->get_mouse_position = [](int& x, int& y) {
            x = input::mouseX.load(std::memory_order_relaxed);
            y = input::mouseY.load(std::memory_order_relaxed);
        };
        sdlContext->is_mouse_button_held = [](input::MouseButton b) { return input::is_mouse_button_held(b); };
        sdlContext->is_mouse_button_down = [](input::MouseButton b) { return input::is_mouse_button_down(b); };

        sdlContext->registry_get = [](const char*) { return 0; };
        sdlContext->draw_sprite = sdltextures::draw_sprite;

        sdlContext->add_texture = [&](TextureAtlas& a, const std::string& n, const ImageData& i) {
            return AddTextureThunk(a, n, i, ContextType::SDL);
            };
        sdlContext->add_atlas = [&](const TextureAtlas& a) {
            return AddAtlasThunk(a, ContextType::SDL);
        };
        sdlContext->add_model = [](const char*, const char*) { return 0; };

        sdlContext->backendName = "SDL";
        sdlContext->type = ContextType::SDL;
        AddContextForBackend(ContextType::SDL, sdlContext);
        atlasmanager::register_backend_uploader(ContextType::SDL,
            [](const TextureAtlas& atlas) {
                sdltextures::ensure_uploaded(atlas);
            });
#endif

#if defined(ALMOND_USING_SFML)
        auto sfmlContext = std::make_shared<Context>();
        sfmlContext->initialize = sfml_initialize;
        sfmlContext->cleanup = sfml_cleanup_adapter;
        sfmlContext->process = almondnamespace::sfmlcontext::sfml_process;
        sfmlContext->clear = almondnamespace::sfmlcontext::sfml_clear;
        sfmlContext->present = almondnamespace::sfmlcontext::sfml_present;
        sfmlContext->get_width = almondnamespace::sfmlcontext::sfml_get_width;
        sfmlContext->get_height = almondnamespace::sfmlcontext::sfml_get_height;

        sfmlContext->is_key_held = [](input::Key k) { return input::is_key_held(k); };
        sfmlContext->is_key_down = [](input::Key k) { return input::is_key_down(k); };
        sfmlContext->get_mouse_position = [](int& x, int& y) {
            x = input::mouseX.load(std::memory_order_relaxed);
            y = input::mouseY.load(std::memory_order_relaxed);
        };
        sfmlContext->is_mouse_button_held = [](input::MouseButton b) { return input::is_mouse_button_held(b); };
        sfmlContext->is_mouse_button_down = [](input::MouseButton b) { return input::is_mouse_button_down(b); };

        sfmlContext->registry_get = [](const char*) { return 0; };
        sfmlContext->draw_sprite = sfmlcontext::draw_sprite;

        sfmlContext->add_texture = [&](TextureAtlas& a, const std::string& n, const ImageData& i) {
            return AddTextureThunk(a, n, i, ContextType::SFML);
            };
        sfmlContext->add_atlas = [&](const TextureAtlas& a) {
            return AddAtlasThunk(a, ContextType::SFML);
        };
        sfmlContext->add_model = [](const char*, const char*) { return 0; };

        sfmlContext->backendName = "SFML";
        sfmlContext->type = ContextType::SFML;
        AddContextForBackend(ContextType::SFML, sfmlContext);
        atlasmanager::register_backend_uploader(ContextType::SFML,
            [](const TextureAtlas& atlas) {
                sfmlcontext::ensure_uploaded(atlas);
            });
#endif

#if defined(ALMOND_USING_RAYLIB)
        auto raylibContext = std::make_shared<Context>();
        raylibContext->initialize = raylib_initialize;
        raylibContext->cleanup = raylib_cleanup_adapter;
        raylibContext->process = almondnamespace::raylibcontext::raylib_process;
        raylibContext->clear = almondnamespace::raylibcontext::raylib_clear;
        raylibContext->present = almondnamespace::raylibcontext::raylib_present;
        raylibContext->get_width = almondnamespace::raylibcontext::raylib_get_width;
        raylibContext->get_height = almondnamespace::raylibcontext::raylib_get_height;

        raylibContext->is_key_held = [](input::Key k) { return input::is_key_held(k); };
        raylibContext->is_key_down = [](input::Key k) { return input::is_key_down(k); };
        raylibContext->get_mouse_position = [](int& x, int& y) {
            almondnamespace::raylibcontext::raylib_get_mouse_position(x, y);
        };
        raylibContext->is_mouse_button_held = [](input::MouseButton b) { return input::is_mouse_button_held(b); };
        raylibContext->is_mouse_button_down = [](input::MouseButton b) { return input::is_mouse_button_down(b); };

        raylibContext->registry_get = [](const char*) { return 0; };
        raylibContext->draw_sprite = raylibcontext::draw_sprite;

        raylibContext->add_texture = [&](TextureAtlas& a, const std::string& n, const ImageData& i) {
            return AddTextureThunk(a, n, i, ContextType::RayLib);
            };
        raylibContext->add_atlas = [&](const TextureAtlas& a) {
            return AddAtlasThunk(a, ContextType::RayLib);
        };
        raylibContext->add_model = [](const char*, const char*) { return 0; };

        raylibContext->backendName = "RayLib";
        raylibContext->type = ContextType::RayLib;
        AddContextForBackend(ContextType::RayLib, raylibContext);
        atlasmanager::register_backend_uploader(ContextType::RayLib,
            [](const TextureAtlas& atlas) {
                raylibtextures::ensure_uploaded(atlas);
            });
#endif

#if defined(ALMOND_USING_VULKAN)
        auto vulkanContext = std::make_shared<Context>();
        vulkanContext->initialize = vulkan_initialize;
        vulkanContext->cleanup = vulkan_cleanup;
        vulkanContext->process = vulkan_process;
        vulkanContext->clear = vulkan_clear;
        vulkanContext->present = vulkan_present;
        vulkanContext->get_width = vulkan_get_width;
        vulkanContext->get_height = vulkan_get_height;

        vulkanContext->is_key_held = [](input::Key k) { return input::is_key_held(k); };
        vulkanContext->is_key_down = [](input::Key k) { return input::is_key_down(k); };
        vulkanContext->get_mouse_position = [](int& x, int& y) {
            x = input::mouseX.load(std::memory_order_relaxed);
            y = input::mouseY.load(std::memory_order_relaxed);
        };
        vulkanContext->is_mouse_button_held = [](input::MouseButton b) { return input::is_mouse_button_held(b); };
        vulkanContext->is_mouse_button_down = [](input::MouseButton b) { return input::is_mouse_button_down(b); };

        vulkanContext->registry_get = [](const char*) { return 0; };
        vulkanContext->draw_sprite = [](SpriteHandle, std::span<const TextureAtlas* const>, float, float, float, float) {};

        vulkanContext->add_texture = [&](TextureAtlas& a, const std::string& n, const ImageData& i) {
            return AddTextureThunk(a, n, i, ContextType::Vulkan);
            };
        vulkanContext->add_atlas = [&](const TextureAtlas& a) {
            return AddAtlasThunk(a, ContextType::Vulkan);
        };
        vulkanContext->add_model = [](const char*, const char*) { return 0; };

        vulkanContext->backendName = "Vulkan";
        vulkanContext->type = ContextType::Vulkan;
        AddContextForBackend(ContextType::Vulkan, vulkanContext);
#endif

#if defined(ALMOND_USING_DIRECTX)
        auto directxContext = std::make_shared<Context>();
        directxContext->initialize = directx_initialize;
        directxContext->cleanup = directx_cleanup;
        directxContext->process = directx_process;
        directxContext->clear = directx_clear;
        directxContext->present = directx_present;
        directxContext->get_width = directx_get_width;
        directxContext->get_height = directx_get_height;

        directxContext->is_key_held = [](input::Key k) { return input::is_key_held(k); };
        directxContext->is_key_down = [](input::Key k) { return input::is_key_down(k); };
        directxContext->get_mouse_position = [](int& x, int& y) {
            x = input::mouseX.load(std::memory_order_relaxed);
            y = input::mouseY.load(std::memory_order_relaxed);
        };
        directxContext->is_mouse_button_held = [](input::MouseButton b) { return input::is_mouse_button_held(b); };
        directxContext->is_mouse_button_down = [](input::MouseButton b) { return input::is_mouse_button_down(b); };

        directxContext->registry_get = [](const char*) { return 0; };
        directxContext->draw_sprite = [](SpriteHandle, std::span<const TextureAtlas* const>, float, float, float, float) {};

        directxContext->add_texture = [&](TextureAtlas& a, const std::string& n, const ImageData& i) {
            return AddTextureThunk(a, n, i, ContextType::DirectX);
            };
        directxContext->add_atlas = [&](const TextureAtlas& a) {
            return AddAtlasThunk(a, ContextType::DirectX);
        };
        directxContext->add_model = [](const char*, const char*) { return 0; };

        directxContext->backendName = "DirectX";
        directxContext->type = ContextType::DirectX;
        AddContextForBackend(ContextType::DirectX, directxContext);
#endif

#if defined(ALMOND_USING_CUSTOM)
        auto customContext = std::make_shared<Context>();
        customContext->initialize = custom_initialize;
        customContext->cleanup = custom_cleanup;
        customContext->process = custom_process;
        customContext->clear = custom_clear;
        customContext->present = custom_present;
        customContext->get_width = custom_get_width;
        customContext->get_height = custom_get_height;

        customContext->is_key_held = [](input::Key k) { return input::is_key_held(k); };
        customContext->is_key_down = [](input::Key k) { return input::is_key_down(k); };
        customContext->get_mouse_position = [](int& x, int& y) {
            x = input::mouseX.load(std::memory_order_relaxed);
            y = input::mouseY.load(std::memory_order_relaxed);
        };
        customContext->is_mouse_button_held = [](input::MouseButton b) { return input::is_mouse_button_held(b); };
        customContext->is_mouse_button_down = [](input::MouseButton b) { return input::is_mouse_button_down(b); };

        customContext->registry_get = [](const char*) { return 0; };
        customContext->draw_sprite = [](SpriteHandle, std::span<const TextureAtlas* const>, float, float, float, float) {};

        customContext->add_texture = [&](TextureAtlas& a, const std::string& n, const ImageData& i) {
            return AddTextureThunk(a, n, i, ContextType::Custom);
            };
        customContext->add_atlas = [&](const TextureAtlas& a) {
            return AddAtlasThunk(a, ContextType::Custom);
        };
        customContext->add_model = [](const char*, const char*) { return 0; };

        customContext->backendName = "Custom";
        customContext->type = ContextType::Custom;
        AddContextForBackend(ContextType::Custom, customContext);
#endif

#if defined(ALMOND_USING_SOFTWARE_RENDERER)
        auto softwareContext = std::make_shared<Context>();
        softwareContext->initialize = softrenderer_initialize;
        softwareContext->cleanup = softrenderer_cleanup_adapter;
        softwareContext->process = almondnamespace::anativecontext::softrenderer_process;
        softwareContext->clear = nullptr;
        softwareContext->present = nullptr;
        softwareContext->get_width = almondnamespace::anativecontext::get_width;
        softwareContext->get_height = almondnamespace::anativecontext::get_height;

        softwareContext->is_key_held = [](input::Key k) { return input::is_key_held(k); };
        softwareContext->is_key_down = [](input::Key k) { return input::is_key_down(k); };
        softwareContext->get_mouse_position = [](int& x, int& y) {
            x = input::mouseX.load(std::memory_order_relaxed);
            y = input::mouseY.load(std::memory_order_relaxed);
        };
        softwareContext->is_mouse_button_held = [](input::MouseButton b) { return input::is_mouse_button_held(b); };
        softwareContext->is_mouse_button_down = [](input::MouseButton b) { return input::is_mouse_button_down(b); };

        softwareContext->registry_get = [](const char*) { return 0; };
        softwareContext->draw_sprite = anativecontext::draw_sprite;

        softwareContext->add_texture = [&](TextureAtlas& a, const std::string& n, const ImageData& i) {
            return AddTextureThunk(a, n, i, ContextType::Software);
            };
        softwareContext->add_atlas = [&](const TextureAtlas& a) {
            return AddAtlasThunk(a, ContextType::Software);
        };
        softwareContext->add_model = [](const char*, const char*) { return 0; };

        softwareContext->backendName = "Software";
        softwareContext->type = ContextType::Software;
        AddContextForBackend(ContextType::Software, softwareContext);
#endif
    }

    bool ProcessAllContexts() {
        bool anyRunning = false;

        std::vector<std::shared_ptr<Context>> contexts;
        {
            std::shared_lock lock(g_backendsMutex);
            contexts.reserve(g_backends.size());
            for (auto& [_, state] : g_backends) {
                if (state.master)
                    contexts.push_back(state.master);
                for (auto& dup : state.duplicates)
                    contexts.push_back(dup);
            }
        }

        auto process_context = [&](const std::shared_ptr<Context>& ctx) {
            if (!ctx) {
                return false;
            }

            if (auto* window = ctx->windowData) {
                if (window->running) {
                    // A dedicated render thread is already driving this context.
                    // Avoid re-entering backend process callbacks from the main thread.
                    return true;
                }

                // The render thread has stopped; drain any residual commands but
                // do not invoke the backend again from this thread.
                return window->commandQueue.drain();
            }

            CommandQueue localQueue;

            if (!ctx->process) {
                return localQueue.drain();
            }

            return ctx->process_safe(ctx, localQueue);
        };

        for (auto& ctx : contexts) {
            if (process_context(ctx)) {
                anyRunning = true;
            }
        }

        return anyRunning;
    }

} // namespace almondnamespace::core
