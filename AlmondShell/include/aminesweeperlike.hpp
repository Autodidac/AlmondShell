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
 // aminesweeperlike.hpp
#pragma once

#include "aplatformpump.hpp"
#include "arobusttime.hpp"
#include "acontext.hpp"
#include "ainput.hpp"
#include "agamecore.hpp"
#include "aatlasmanager.hpp"
#include "aimageloader.hpp"
#include "ascene.hpp"
#include "aspritepool.hpp"

#include <vector>
#include <chrono>
#include <tuple>
#include <unordered_map>
#include <string>
#include <random>
#include <iostream>
#include <span>
#include <stdexcept>

namespace almondnamespace::minesweeper
{
    constexpr int GRID_W = 16;
    constexpr int GRID_H = 16;
    constexpr int MINES = 40;

    struct GameState
    {
        gamecore::grid_t<bool> mine;
        gamecore::grid_t<bool> revealed;
        gamecore::grid_t<int>  count;

        GameState()
            : mine(gamecore::make_grid<bool>(GRID_W, GRID_H, false)),
            revealed(gamecore::make_grid<bool>(GRID_W, GRID_H, false)),
            count(gamecore::make_grid<int>(GRID_W, GRID_H, 0))
        {
            int placed = 0;
            std::mt19937_64 rng{ std::random_device{}() };
            std::uniform_int_distribution<int> dx(0, GRID_W - 1), dy(0, GRID_H - 1);
            while (placed < MINES)
            {
                int x = dx(rng), y = dy(rng);
                auto idx = gamecore::idx(GRID_W, x, y);
                if (!mine[idx]) { mine[idx] = true; ++placed; }
            }

            for (int y = 0; y < GRID_H; ++y)
                for (int x = 0; x < GRID_W; ++x)
                    if (!mine[gamecore::idx(GRID_W, x, y)])
                    {
                        int c = 0;
                        for (auto [nx, ny] : gamecore::neighbors(GRID_W, GRID_H, x, y))
                            if (mine[gamecore::idx(GRID_W, nx, ny)]) ++c;
                        count[gamecore::idx(GRID_W, x, y)] = c;
                    }
        }

        bool all_clear() const
        {
            for (size_t i = 0; i < revealed.size(); ++i)
                if (!revealed[i] && !mine[i]) return false;
            return true;
        }
    };

    struct MinesweeperScene : public scene::Scene {
        MinesweeperScene(Logger* L = nullptr, time::Timer* C = nullptr)
            : Scene(L, C)
        {
        }

        void load() override {
            Scene::load();
            setupSprites();
            state = {};
            gameOver = false;
            mouseWasDown = false;
        }

        bool frame(std::shared_ptr<core::Context> ctx, core::WindowData*) override {
            if (!ctx)
                return false;

            platform::pump_events();

            if (ctx->is_key_down_safe(input::Key::Escape))
                return false;

            int mx = 0;
            int my = 0;
            ctx->get_mouse_position_safe(mx, my);
            const bool mouseDown = ctx->is_mouse_button_down_safe(input::MouseButton::MouseLeft);

            if (mouseDown && !mouseWasDown) {
                const int gx = int(mx / (float(ctx->get_width_safe()) / GRID_W));
                const int gy = int(my / (float(ctx->get_height_safe()) / GRID_H));

                if (gamecore::in_bounds(GRID_W, GRID_H, gx, gy)) {
                    const auto idx = gamecore::idx(GRID_W, gx, gy);
                    if (!state.revealed[idx]) {
                        state.revealed[idx] = true;
                        if (state.mine[idx]) {
                            gameOver = true;
                        }
                        else if (state.count[idx] == 0) {
                            auto flood = [&](auto&& self, int fx, int fy) -> void {
                                if (!gamecore::in_bounds(GRID_W, GRID_H, fx, fy))
                                    return;
                                const auto i = gamecore::idx(GRID_W, fx, fy);
                                if (state.revealed[i])
                                    return;
                                state.revealed[i] = true;
                                if (state.count[i] == 0)
                                    for (auto [nx, ny] : gamecore::neighbors(GRID_W, GRID_H, fx, fy))
                                        self(self, int(nx), int(ny));
                            };
                            flood(flood, gx, gy);
                        }
                    }
                }
            }

            mouseWasDown = mouseDown;

            ctx->clear_safe(ctx);

            const float cellW = float(ctx->get_width_safe()) / GRID_W;
            const float cellH = float(ctx->get_height_safe()) / GRID_H;

            auto& atlasVec = atlasmanager::get_atlas_vector();
            std::span<const TextureAtlas* const> atlasSpan(atlasVec.data(), atlasVec.size());

            for (int y = 0; y < GRID_H; ++y) {
                for (int x = 0; x < GRID_W; ++x) {
                    const size_t idx = gamecore::idx(GRID_W, x, y);
                    const std::string key = !state.revealed[idx]
                        ? "covered"
                        : (state.mine[idx] ? "mine" : std::to_string(state.count[idx]));

                    auto it = sprites.find(key);
                    if (it != sprites.end() && spritepool::is_alive(it->second)) {
                        ctx->draw_sprite_safe(it->second, atlasSpan,
                            x * cellW, y * cellH, cellW, cellH);
                    }
                }
            }

            ctx->present_safe();

            if (gameOver)
                return false;
            if (state.all_clear())
                return false;

            return true;
        }

        void unload() override {
            Scene::unload();
            sprites.clear();
            gameOver = false;
            mouseWasDown = false;
        }

    private:
        void setupSprites() {
            sprites.clear();

            const bool createdAtlas = atlasmanager::create_atlas({
                .name = "minesweeper_atlas",
                .width = 512,
                .height = 512,
                .generate_mipmaps = false });

            auto* registrar = atlasmanager::get_registrar("minesweeper_atlas");
            if (!registrar)
                throw std::runtime_error("[Minesweeper] Missing atlas registrar");

            TextureAtlas& atlas = registrar->atlas;
            bool registeredSprite = false;

            auto ensureSprite = [&](std::string_view id) {
                std::string name(id);
                if (auto existing = atlasmanager::registry.get(name)) {
                    auto handle = std::get<0>(*existing);
                    if (spritepool::is_alive(handle)) {
                        sprites[name] = handle;
                        return;
                    }
                }

                auto img = a_loadImage("assets/minesweeperlike/" + name + ".ppm", false);
                if (img.pixels.empty())
                    throw std::runtime_error("[Minesweeper] Missing image " + name);

                auto handleOpt = registrar->register_atlas_sprites_by_image(
                    name, img.pixels, img.width, img.height, atlas);
                if (!handleOpt || !spritepool::is_alive(*handleOpt))
                    throw std::runtime_error("[Minesweeper] Failed to register sprite " + name);

                sprites[name] = *handleOpt;
                registeredSprite = true;
            };

            for (int i = 0; i <= 8; ++i)
                ensureSprite(std::to_string(i));

            ensureSprite("covered");
            ensureSprite("mine");

            if (createdAtlas || registeredSprite) {
                atlas.rebuild_pixels();
                atlasmanager::ensure_uploaded(atlas);
            }
        }

        GameState state{};
        std::unordered_map<std::string, SpriteHandle> sprites{};
        bool gameOver = false;
        bool mouseWasDown = false;
    };

    inline bool run_minesweeper(std::shared_ptr<core::Context> ctx)
    {
        MinesweeperScene scene;
        scene.load();

        auto* window = ctx ? ctx->windowData : nullptr;
        bool running = true;

        while (running && ctx)
            running = scene.frame(ctx, window);

        scene.unload();
        return running;
    }
} // namespace almondnamespace::minesweeper
