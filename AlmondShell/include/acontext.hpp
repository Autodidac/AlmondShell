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
 // acontext.hpp
#pragma once

//#include "aplatform.hpp"        // Must always come first for platform defines
//#include "aengineconfig.hpp"    // All ENGINE-specific includes

#include "ainput.hpp"           // Keycodes and input handling
#include "aatlastexture.hpp"    // TextureAtlas type
#include "aspritehandle.hpp"    // SpriteHandle type
#include "aatomicfunction.hpp"  // AlmondAtomicFunction
#include "acommandqueue.hpp"    // CommandQueue
//#include "awindowdata.hpp"      // WindowData
#include "acontexttype.hpp"     // ContextType enum

#include <algorithm>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <span>
#include <functional>
#include <cstdint>
#include <mutex>
#include <queue>
#include <shared_mutex>

#if defined(__linux__)
namespace almondnamespace::platform {
    extern Display* global_display;
    extern ::Window global_window;
}
#endif

namespace almondnamespace::core
{
    struct WindowData; // forward declaration

    // ======================================================
    // Context: core per-backend state
    // Each backend (OpenGL, SDL, Raylib, etc.) wires its
    // rendering + input hooks here.
    // ======================================================
    struct Context {
        // --- Backend render hooks (fast raw function pointers) ---
        using InitializeFunc = void(*)();
        using CleanupFunc = void(*)();
        using ProcessFunc = bool(*)(std::shared_ptr<core::Context>, CommandQueue&);
        using ClearFunc = void(*)();
        using PresentFunc = void(*)();
        using GetWidthFunc = int(*)();
        using GetHeightFunc = int(*)();
        using RegistryGetFunc = int(*)(const char*);
        using DrawSpriteFunc = void(*)(SpriteHandle, std::span<const TextureAtlas* const>, float, float, float, float);
        using AddTextureFunc = uint32_t(*)(TextureAtlas&, std::string, const ImageData&);
        using AddAtlasFunc = uint32_t(*)(const TextureAtlas&);
        using AddModelFunc = int(*)(const char*, const char*);

#if defined(_WIN32) || defined(WIN32)
        HWND hwnd = nullptr;
        HDC  hdc = nullptr;
        HGLRC hglrc = nullptr;
#else
        void* hwnd = nullptr;
        void* hdc = nullptr;
        void* hglrc = nullptr;
#endif
        WindowData* windowData = nullptr;

        // `width`/`height` track the active logical canvas that game/UI code
        // should use for layout. They are the coordinates returned by
        // `get_width_safe()`/`get_height_safe()` and match the virtual canvas
        // when a backend exposes one. Backends that do not distinguish between
        // logical and physical pixels should keep these in lock-step with the
        // framebuffer dimensions.
        int width = 400;
        int height = 300;

        // `framebufferWidth`/`framebufferHeight` report the most recent pixel
        // size of the drawable surface. High-DPI backends (Raylib, SDL, etc.)
        // should refresh these whenever they receive resize notifications so
        // diagnostics and renderer code can reason about the physical target.
        int framebufferWidth = 400;
        int framebufferHeight = 300;

        // `virtualWidth`/`virtualHeight` cache the design-time canvas that the
        // logical coordinates map onto. When a backend supports virtualised
        // rendering it can diverge from the framebuffer dimensions; otherwise
        // these simply mirror the logical width/height.
        int virtualWidth = 400;
        int virtualHeight = 300;
        ContextType type = almondnamespace::core::ContextType::Custom;
        std::string backendName;

        // --- Backend function pointers ---
        InitializeFunc initialize = nullptr;
        CleanupFunc cleanup = nullptr;
        ProcessFunc process = nullptr;
        ClearFunc clear = nullptr;
        PresentFunc present = nullptr;
        GetWidthFunc get_width = nullptr;
        GetHeightFunc get_height = nullptr;
        RegistryGetFunc registry_get = nullptr;
        DrawSpriteFunc draw_sprite = nullptr;
        AddModelFunc add_model = nullptr;

        // --- Input hooks (std::function for flexibility) ---
        std::function<bool(input::Key)> is_key_held;
        std::function<bool(input::Key)> is_key_down;
        std::function<void(int&, int&)> get_mouse_position;
        std::function<bool(input::MouseButton)> is_mouse_button_held;
        std::function<bool(input::MouseButton)> is_mouse_button_down;

        // --- High-level callbacks ---
        AlmondAtomicFunction<uint32_t(TextureAtlas&, std::string, const ImageData&)> add_texture;
        AlmondAtomicFunction<uint32_t(const TextureAtlas&)> add_atlas;
        std::function<void(int, int)> onResize;

        Context() = default;

        Context(const std::string& name, ContextType t,
            InitializeFunc init, CleanupFunc cln, ProcessFunc proc,
            ClearFunc clr, PresentFunc pres, GetWidthFunc gw, GetHeightFunc gh,
            RegistryGetFunc rg, DrawSpriteFunc ds,
            AddTextureFunc at, AddAtlasFunc aa, AddModelFunc am)
            : width(400), height(300), framebufferWidth(400), framebufferHeight(300),
            virtualWidth(400), virtualHeight(300),
            type(t), backendName(name),
            initialize(init), cleanup(cln), process(proc), clear(clr),
            present(pres), get_width(gw), get_height(gh),
            registry_get(rg), draw_sprite(ds), add_model(am)
        {
            add_texture.store(at);
            add_atlas.store(aa);
        }

        // --- Safe wrappers ---
        inline void initialize_safe() const noexcept { if (initialize) initialize(); }
        inline void cleanup_safe()   const noexcept { if (cleanup) cleanup(); }
        bool process_safe(std::shared_ptr<core::Context> ctx, CommandQueue& queue);

        inline void clear_safe(std::shared_ptr<Context>) const noexcept { if (clear) clear(); }
        inline void present_safe() const noexcept { if (present) present(); }

        inline int  get_width_safe()  const noexcept { return get_width ? get_width() : width; }
        inline int  get_height_safe() const noexcept { return get_height ? get_height() : height; }

        inline bool is_key_held_safe(input::Key k) const noexcept {
            return is_key_held ? is_key_held(k) : false;
        }
        inline bool is_key_down_safe(input::Key k) const noexcept {
            return is_key_down ? is_key_down(k) : false;
        }
        inline void get_mouse_position_safe(int& x, int& y) const noexcept {
            if (get_mouse_position) {
                get_mouse_position(x, y);
            }
            else {
                x = input::mouseX.load(std::memory_order_acquire);
                y = input::mouseY.load(std::memory_order_acquire);
            }

#if defined(_WIN32)
            if (hwnd && input::are_mouse_coords_global()) {
                POINT pt{ x, y };
                if (ScreenToClient(hwnd, &pt)) {
                    RECT rc{};
                    if (GetClientRect(hwnd, &rc) &&
                        (pt.x < rc.left || pt.x >= rc.right ||
                         pt.y < rc.top || pt.y >= rc.bottom)) {
                        x = -1;
                        y = -1;
                    }
                    else {
                        x = pt.x;
                        y = pt.y;
                    }
                }
                else {
                    x = -1;
                    y = -1;
                }
            }
#elif defined(__linux__)
            if (hwnd && input::are_mouse_coords_global()) {
                Display* display = almondnamespace::platform::global_display;
                ::Window target = static_cast<::Window>(reinterpret_cast<uintptr_t>(hwnd));
                if (display && target != 0) {
                    Window root{};
                    Window child{};
                    int rootX = 0;
                    int rootY = 0;
                    int winX = 0;
                    int winY = 0;
                    unsigned int mask = 0;

                    if (XQueryPointer(display, target, &root, &child, &rootX, &rootY, &winX, &winY, &mask)) {
                        const int widthClamp = (std::max)(1, width);
                        const int heightClamp = (std::max)(1, height);
                        if (winX < 0 || winY < 0 || winX >= widthClamp || winY >= heightClamp) {
                            x = -1;
                            y = -1;
                        }
                        else {
                            x = winX;
                            y = winY;
                        }
                    }
                    else {
                        x = -1;
                        y = -1;
                    }
                }
                else {
                    x = -1;
                    y = -1;
                }
            }
#endif
        }
        inline bool is_mouse_button_held_safe(input::MouseButton b) const noexcept {
            if (is_mouse_button_held) {
                return is_mouse_button_held(b);
            }
            return input::is_mouse_button_held(b);
        }
        inline bool is_mouse_button_down_safe(input::MouseButton b) const noexcept {
            if (is_mouse_button_down) {
                return is_mouse_button_down(b);
            }
            return input::is_mouse_button_down(b);
        }

        inline int registry_get_safe(const char* key) const noexcept {
            return registry_get ? registry_get(key) : 0;
        }

        inline void draw_sprite_safe(SpriteHandle h,
            std::span<const TextureAtlas* const> atlases,
            float x, float y, float w, float hgt) const noexcept {
            if (draw_sprite) draw_sprite(h, atlases, x, y, w, hgt);
        }

        inline uint32_t add_texture_safe(TextureAtlas& atlas,
            std::string name,
            const ImageData& img) const noexcept {
            try { return add_texture(atlas, name, img); }
            catch (...) { return 0u; }
        }

        inline uint32_t add_atlas_safe(const TextureAtlas& atlas) const noexcept {
            try { return add_atlas(atlas); }
            catch (...) { return 0u; }
        }

        inline int add_model_safe(const char* name, const char* path) const noexcept {
            return add_model ? add_model(name, path) : -1;
        }
    };

    // ======================================================
    // BackendState: holds master + duplicates for a backend
    // ======================================================
    struct BackendState {
        std::shared_ptr<Context> master;
        std::vector<std::shared_ptr<Context>> duplicates;

        // opaque per-backend storage (cast in backends)
        std::unique_ptr<void, void(*)(void*)> data{ nullptr, [](void*) {} };
    };

    using BackendMap = std::map<ContextType, BackendState>;
    extern BackendMap g_backends;
    extern std::shared_mutex g_backendsMutex;

    void InitializeAllContexts();
    std::shared_ptr<Context> CloneContext(const Context& prototype);
    void AddContextForBackend(ContextType type, std::shared_ptr<Context> context);
    bool ProcessAllContexts();

} // namespace almondnamespace::core
