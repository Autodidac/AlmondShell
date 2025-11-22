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
 // asandsim.hpp
#pragma once

#include "acontext.hpp"
#include "agamecore.hpp"
#include "ainput.hpp"
#include "aplatformpump.hpp"
#include "arobusttime.hpp"
#include "aatlasmanager.hpp"
#include "aspritepool.hpp"
#include "ascene.hpp"
#include "aimageloader.hpp" // For a_loadImage

#include <chrono>
#include <span>
#include <iostream>
#include <stdexcept>

namespace almondnamespace::sandsim
{
    static SpriteRegistry registry;

    constexpr int W = 120, H = 80;
    constexpr double STEP_S = 0.016;

    struct SandSimScene : public scene::Scene {
        SandSimScene(Logger* L = nullptr, time::Timer* C = nullptr)
            : Scene(L, C)
        {
        }

        void load() override {
            Scene::load();
            setupSprites();
            grid = gamecore::make_grid<bool>(W, H, false);
            timer = time::createTimer(0.25);
            time::setScale(timer, 0.25);
            acc = 0.0;
        }

        bool frame(std::shared_ptr<core::Context> ctx, core::WindowData*) override {
            if (!ctx)
                return false;

            platform::pump_events();
            if (ctx->is_key_down_safe(input::Key::Escape))
                return false;

            int mx = 0, my = 0;
            ctx->get_mouse_position_safe(mx, my);
            const int w = (std::max)(1, ctx->get_width_safe());
            const int h = (std::max)(1, ctx->get_height_safe());

            const float sx = W / float(w);
            const float sy = H / float(h);

            int gx = int(mx * sx);
            int gy = int(my * sy);
            gx = std::clamp(gx, 0, W - 1);
            gy = std::clamp(gy, 0, H - 1);

            if (ctx->is_mouse_button_down_safe(input::MouseButton::MouseLeft) &&
                gamecore::in_bounds(W, H, gx, gy)) {
                gamecore::at(grid, W, H, gx, gy) = true;
            }

            advance(timer, STEP_S);
            acc += time::elapsed(timer);
            reset(timer);

            while (acc >= STEP_S) {
                acc -= STEP_S;
                stepSimulation();
            }

            ctx->clear_safe(ctx);
            auto& atlasVec = atlasmanager::get_atlas_vector();
            std::span<const TextureAtlas* const> atlasSpan(atlasVec.data(), atlasVec.size());

            const float cw = float(ctx->get_width_safe()) / W;
            const float ch = float(ctx->get_height_safe()) / H;

            if (!spritepool::is_alive(sandHandle))
                return true;

            for (int y = 0; y < H; ++y) {
                for (int x = 0; x < W; ++x) {
                    if (gamecore::at(grid, W, H, x, y)) {
                        ctx->draw_sprite_safe(sandHandle, atlasSpan,
                            x * cw, y * ch, cw, ch);
                    }
                }
            }

            ctx->present_safe();
            return true;
        }

        void unload() override {
            Scene::unload();
            grid.clear();
            acc = 0.0;
        }

    private:
        void setupSprites() {
            const bool createdAtlas = atlasmanager::create_atlas({
                .name = "sand_atlas",
                .width = 64,
                .height = 64,
                .generate_mipmaps = false });

            auto* registrar = atlasmanager::get_registrar("sand_atlas");
            if (!registrar)
                throw std::runtime_error("[SandSim] Missing atlas registrar");

            TextureAtlas& atlas = registrar->atlas;
            bool registered = false;

            if (auto existing = atlasmanager::registry.get("sand")) {
                auto handle = std::get<0>(*existing);
                if (spritepool::is_alive(handle)) {
                    sandHandle = handle;
                }
            }

            if (!spritepool::is_alive(sandHandle)) {
                auto img = a_loadImage("assets/sand/sand.ppm", false);
                if (img.pixels.empty())
                    throw std::runtime_error("[SandSim] Failed to load sand.ppm");

                auto handleOpt = registrar->register_atlas_sprites_by_image(
                    "sand", img.pixels, img.width, img.height, atlas);
                if (!handleOpt || !spritepool::is_alive(*handleOpt))
                    throw std::runtime_error("[SandSim] Failed to register sand sprite");

                sandHandle = *handleOpt;
                registered = true;
            }

            if (createdAtlas || registered) {
                atlas.rebuild_pixels();
                atlasmanager::ensure_uploaded(atlas);
            }
        }

        void stepSimulation() {
            auto next = grid;
            for (int y = H - 1; y >= 0; --y) {
                for (int x = 0; x < W; ++x) {
                    if (gamecore::at(grid, W, H, x, y)) {
                        if (gamecore::is_free(grid, W, H, x, y + 1, false)) {
                            gamecore::at(next, W, H, x, y + 1) = true;
                            gamecore::at(next, W, H, x, y) = false;
                        }
                        else if (gamecore::is_free(grid, W, H, x - 1, y + 1, false)) {
                            gamecore::at(next, W, H, x - 1, y + 1) = true;
                            gamecore::at(next, W, H, x, y) = false;
                        }
                        else if (gamecore::is_free(grid, W, H, x + 1, y + 1, false)) {
                            gamecore::at(next, W, H, x + 1, y + 1) = true;
                            gamecore::at(next, W, H, x, y) = false;
                        }
                    }
                }
            }
            grid = std::move(next);
        }

        gamecore::grid_t<bool> grid{};
        SpriteHandle sandHandle{};
        time::Timer timer{};
        double acc = 0.0;
    };

    inline bool run_sand(std::shared_ptr<core::Context> ctx)
    {
        SandSimScene scene;
        scene.load();

        auto* window = ctx ? ctx->windowData : nullptr;
        bool running = true;
        while (running && ctx)
            running = scene.frame(ctx, window);

        scene.unload();
        return running;
    }

} // namespace almondnamespace::sandsim
