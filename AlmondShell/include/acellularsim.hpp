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
 // acellularsim.hpp
#pragma once

#include "acontext.hpp"
#include "agamecore.hpp"
#include "ainput.hpp"
#include "aplatformpump.hpp"
#include "arobusttime.hpp"
#include "aatlasmanager.hpp"
#include "aspritepool.hpp"
#include "ascene.hpp"
#include "aimageloader.hpp"

#include <random>
#include <iostream>
#include <vector>
#include <span>
#include <stdexcept>

namespace almondnamespace::cellular {

    constexpr int W = 80, H = 60;

    struct CellularScene : public scene::Scene {
        CellularScene(Logger* L = nullptr, time::Timer* C = nullptr)
            : Scene(L, C)
        {
        }

        void load() override {
            Scene::load();
            setupSprites();
            grid = gamecore::make_grid<bool>(W, H, false);
            seedRandom();
        }

        bool frame(std::shared_ptr<core::Context> ctx, core::WindowData*) override {
            if (!ctx)
                return false;

            platform::pump_events();
            if (ctx->is_key_down_safe(input::Key::Escape))
                return false;

            stepSimulation();

            ctx->clear_safe(ctx);
            auto& atlasVec = atlasmanager::get_atlas_vector();
            std::span<const TextureAtlas* const> atlasSpan(atlasVec.data(), atlasVec.size());

            const float cw = float(ctx->get_width_safe()) / W;
            const float ch = float(ctx->get_height_safe()) / H;

            if (!spritepool::is_alive(cellHandle))
                return true;

            for (int y = 0; y < H; ++y) {
                for (int x = 0; x < W; ++x) {
                    if (grid[gamecore::idx(W, x, y)])
                        ctx->draw_sprite_safe(cellHandle, atlasSpan,
                            x * cw, y * ch, cw, ch);
                }
            }

            ctx->present_safe();
            return true;
        }

        void unload() override {
            Scene::unload();
            grid.clear();
        }

    private:
        void setupSprites() {
            const bool createdAtlas = atlasmanager::create_atlas({
                .name = "cell_atlas",
                .width = 64,
                .height = 64,
                .generate_mipmaps = false });

            auto* registrar = atlasmanager::get_registrar("cell_atlas");
            if (!registrar)
                throw std::runtime_error("[Cellular] Missing registrar");

            TextureAtlas& atlas = registrar->atlas;
            bool registered = false;

            if (auto existing = atlasmanager::registry.get("cell")) {
                auto handle = std::get<0>(*existing);
                if (spritepool::is_alive(handle))
                    cellHandle = handle;
            }

            if (!spritepool::is_alive(cellHandle)) {
                auto img = a_loadImage("assets/cellular/cell.ppm", false);
                if (img.pixels.empty())
                    throw std::runtime_error("[Cellular] Missing cell.ppm");

                auto handleOpt = registrar->register_atlas_sprites_by_image(
                    "cell", img.pixels, img.width, img.height, atlas);
                if (!handleOpt || !spritepool::is_alive(*handleOpt))
                    throw std::runtime_error("[Cellular] Failed to register 'cell' sprite");

                cellHandle = *handleOpt;
                registered = true;
            }

            if (createdAtlas || registered) {
                atlas.rebuild_pixels();
                atlasmanager::ensure_uploaded(atlas);
            }
        }

        void seedRandom() {
            std::mt19937_64 rng{ std::random_device{}() };
            std::bernoulli_distribution d{ 0.2 };
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    grid[gamecore::idx(W, x, y)] = d(rng);
        }

        void stepSimulation() {
            auto next = grid;
            for (int y = 0; y < H; ++y) {
                for (int x = 0; x < W; ++x) {
                    int live = 0;
                    for (auto [nx, ny] : gamecore::neighbors(W, H, x, y))
                        if (grid[gamecore::idx(W, nx, ny)])
                            ++live;
                    bool alive = grid[gamecore::idx(W, x, y)];
                    next[gamecore::idx(W, x, y)] = (live == 3) || (alive && live == 2);
                }
            }
            grid = std::move(next);
        }

        gamecore::grid_t<bool> grid{};
        SpriteHandle cellHandle{};
    };

    inline bool run_cellular(std::shared_ptr<core::Context> ctx)
    {
        CellularScene scene;
        scene.load();

        auto* window = ctx ? ctx->windowData : nullptr;
        bool running = true;
        while (running && ctx)
            running = scene.frame(ctx, window);

        scene.unload();
        return running;
    }

} // namespace almondnamespace::cellular
