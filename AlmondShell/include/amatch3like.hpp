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
 // amatch3game.hpp
#pragma once

#include "ascene.hpp"
#include "aeventsystem.hpp"
#include "aplatformpump.hpp"
#include "acontext.hpp"
#include "ainput.hpp"
#include "agamecore.hpp"
#include "aatlasmanager.hpp"
#include "aimageloader.hpp"
#include "aspritepool.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <random>
#include <span>
#include <string>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace almondnamespace::match3
{
    inline constexpr int GRID_W = 8;
    inline constexpr int GRID_H = 8;
    inline constexpr int MAX_GEM_TYPE = 5;

    struct GameState {
        gamecore::grid_t<int> grid{ gamecore::make_grid<int>(GRID_W, GRID_H, 0) };
        int selectedX = -1;
        int selectedY = -1;
    };

    struct Match3Scene : public scene::Scene {
        Match3Scene(Logger* L = nullptr, time::Timer* C = nullptr)
            : Scene(L, C)
        {
        }

        void load() override {
            Scene::load();
            setupSprites();
            resetBoard();
            mouseWasDown = false;
        }

        bool frame(std::shared_ptr<core::Context> ctx, core::WindowData*) override {
            if (!ctx) return false;

           // input::poll_input();
            events::pump();

            if (ctx->is_key_held(input::Key::Escape))
                return false;

            handleMouse(ctx);
            render(ctx);
            return true;
        }

        void unload() override {
            Scene::unload();
            gemHandles.fill(SpriteHandle{});
            mouseWasDown = false;
        }

    private:
        std::array<SpriteHandle, MAX_GEM_TYPE + 1> gemHandles{};
        GameState state{};
        bool mouseWasDown = false;
        std::mt19937_64 rng{ std::random_device{}() };
        std::uniform_int_distribution<int> gemDist{ 0, MAX_GEM_TYPE };

        void setupSprites() {
            const bool createdAtlas = atlasmanager::create_atlas({
                .name = "match3_atlas",
                .width = 512,
                .height = 512,
                .generate_mipmaps = false
                });

            auto* registrar = atlasmanager::get_registrar("match3_atlas");
            if (!registrar)
                throw std::runtime_error("[Match3] Missing atlas registrar");

            TextureAtlas& atlas = registrar->atlas;
            bool registeredSprite = false;

            for (int i = 0; i <= MAX_GEM_TYPE; ++i) {
                const std::string name = "gem" + std::to_string(i);

                if (auto existing = atlasmanager::registry.get(name)) {
                    auto handle = std::get<0>(*existing);
                    if (spritepool::is_alive(handle)) {
                        gemHandles[i] = handle;
                        continue;
                    }
                }

                auto img = a_loadImage("assets/match3/" + name + ".ppm", false);
                if (img.pixels.empty())
                    throw std::runtime_error("[Match3] Failed to load sprite: " + name);

                auto handleOpt = registrar->register_atlas_sprites_by_image(
                    name, img.pixels, img.width, img.height, atlas);
                if (!handleOpt || !spritepool::is_alive(*handleOpt))
                    throw std::runtime_error("[Match3] Failed to register sprite: " + name);

                gemHandles[i] = *handleOpt;
                registeredSprite = true;
            }

            if (registeredSprite || createdAtlas) {
                atlas.rebuild_pixels();
                atlasmanager::ensure_uploaded(atlas);
            }
        }

        void resetBoard() {
            if (state.grid.empty())
                state.grid = gamecore::make_grid<int>(GRID_W, GRID_H, 0);

            for (int y = 0; y < GRID_H; ++y) {
                for (int x = 0; x < GRID_W; ++x) {
                    state.grid[gamecore::idx(GRID_W, x, y)] = gemDist(rng);
                }
            }
            state.selectedX = -1;
            state.selectedY = -1;
        }

        void handleMouse(const std::shared_ptr<core::Context>& ctx) {
            int mx = 0, my = 0;
            ctx->get_mouse_position_safe(mx, my);
            const bool mouseDown = ctx->is_mouse_button_down_safe(input::MouseButton::MouseLeft);

            if (mouseDown && !mouseWasDown) {
                const float cellW = float(ctx->get_width_safe()) / GRID_W;
                const float cellH = float(ctx->get_height_safe()) / GRID_H;
                const int gx = static_cast<int>(mx / cellW);
                const int gy = static_cast<int>(my / cellH);

                if (gamecore::in_bounds(GRID_W, GRID_H, gx, gy)) {
                    if (state.selectedX == -1) {
                        state.selectedX = gx;
                        state.selectedY = gy;
                    }
                    else {
                        const int manhattan = std::abs(gx - state.selectedX) + std::abs(gy - state.selectedY);
                        if (manhattan == 1) {
                            std::swap(
                                state.grid[gamecore::idx(GRID_W, gx, gy)],
                                state.grid[gamecore::idx(GRID_W, state.selectedX, state.selectedY)]);
                            state.selectedX = -1;
                            state.selectedY = -1;
                        }
                        else {
                            state.selectedX = gx;
                            state.selectedY = gy;
                        }
                    }
                }
            }

            mouseWasDown = mouseDown;
        }

        void render(const std::shared_ptr<core::Context>& ctx) {
            ctx->clear_safe(ctx);

            const float cellW = float(ctx->get_width_safe()) / GRID_W;
            const float cellH = float(ctx->get_height_safe()) / GRID_H;

            auto& atlasVec = atlasmanager::get_atlas_vector();
            std::span<const TextureAtlas* const> atlasSpan(atlasVec.data(), atlasVec.size());

            for (int y = 0; y < GRID_H; ++y) {
                for (int x = 0; x < GRID_W; ++x) {
                    const int type = state.grid[gamecore::idx(GRID_W, x, y)];
                    if (type < 0 || type > MAX_GEM_TYPE) continue;

                    const auto& handle = gemHandles[type];
                    if (!spritepool::is_alive(handle)) continue;

                    ctx->draw_sprite_safe(handle, atlasSpan,
                        x * cellW, y * cellH, cellW, cellH);
                }
            }

            ctx->present_safe();
        }
    };

    inline bool run_match3(std::shared_ptr<core::Context> ctx)
    {
        Match3Scene scene;
        scene.load();
        auto* window = ctx ? ctx->windowData : nullptr;
        bool running = true;

        while (running && ctx) {
            platform::pump_events();
            running = scene.frame(ctx, window);
        }

        scene.unload();
        return running;
    }
}
