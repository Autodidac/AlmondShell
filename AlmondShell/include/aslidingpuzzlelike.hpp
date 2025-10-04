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
 // aslidingpuzzlelike.hpp
#pragma once

#include "acontext.hpp"
#include "agamecore.hpp"
#include "ainput.hpp"
#include "aplatformpump.hpp"
#include "arobusttime.hpp"
#include "aatlasmanager.hpp"
#include "aspritepool.hpp"
#include "aimageloader.hpp"
#include "ascene.hpp"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>
#include <optional>
#include <iostream>
#include <random>
#include <unordered_map>
#include <span>
#include <stdexcept>

namespace almondnamespace::sliding
{
    constexpr int GRID_W = 4;
    constexpr int GRID_H = 4;
    constexpr double MOVE_S = 0.15; // 150 ms per move

    struct SlidingScene : public scene::Scene {
        SlidingScene(Logger* L = nullptr, time::Timer* C = nullptr)
            : Scene(L, C)
        {
        }

        void load() override {
            Scene::load();
            setupSprites();
            state = {};
            timer = time::createTimer(0.25);
            time::setScale(timer, 0.25);
            acc = 0.0;
            movedThisFrame = false;
        }

        bool frame(std::shared_ptr<core::Context> ctx, core::WindowData*) override {
            if (!ctx)
                return false;

            platform::pump_events();

            if (ctx->is_key_down_safe(input::Key::Escape))
                return false;

            int dx = 0;
            int dy = 0;
            if (ctx->is_key_down_safe(input::Key::Left))       dx = -1;
            else if (ctx->is_key_down_safe(input::Key::Right)) dx = 1;
            else if (ctx->is_key_down_safe(input::Key::Up))    dy = -1;
            else if (ctx->is_key_down_safe(input::Key::Down))  dy = 1;

            if (!movedThisFrame && (dx != 0 || dy != 0)) {
                int emptyX = -1;
                int emptyY = -1;
                for (int y = 0; y < GRID_H; ++y) {
                    for (int x = 0; x < GRID_W; ++x) {
                        if (state.grid[y * GRID_W + x] == 0) {
                            emptyX = x;
                            emptyY = y;
                            break;
                        }
                    }
                    if (emptyX != -1)
                        break;
                }

                int fromX = emptyX + dx;
                int fromY = emptyY + dy;

                if (fromX >= 0 && fromX < GRID_W && fromY >= 0 && fromY < GRID_H) {
                    std::swap(state.grid[emptyY * GRID_W + emptyX],
                        state.grid[fromY * GRID_W + fromX]);
                    movedThisFrame = true;
                }
            }

            advance(timer, 0.016);
            acc += time::elapsed(timer);
            reset(timer);

            if (acc >= MOVE_S) {
                acc -= MOVE_S;
                movedThisFrame = false;
            }

            ctx->clear_safe(ctx);

            const float cellW = float(ctx->get_width_safe()) / GRID_W;
            const float cellH = float(ctx->get_height_safe()) / GRID_H;

            auto& atlasVec = atlasmanager::get_atlas_vector();
            std::span<const TextureAtlas* const> atlasSpan(atlasVec.data(), atlasVec.size());

            for (int y = 0; y < GRID_H; ++y) {
                for (int x = 0; x < GRID_W; ++x) {
                    const int tileId = state.grid[y * GRID_W + x];
                    if (tileId == 0)
                        continue;

                    auto it = tiles.find(tileId);
                    if (it != tiles.end() && spritepool::is_alive(it->second)) {
                        ctx->draw_sprite_safe(it->second, atlasSpan,
                            x * cellW, y * cellH, cellW, cellH);
                    }
                }
            }

            ctx->present_safe();
            return true;
        }

        void unload() override {
            Scene::unload();
            tiles.clear();
            acc = 0.0;
            movedThisFrame = false;
        }

    private:
        struct State {
            gamecore::grid_t<int> grid;

            State()
                : grid(gamecore::make_grid<int>(GRID_W, GRID_H, 0))
            {
                std::vector<int> values(GRID_W * GRID_H);
                for (int i = 0; i < static_cast<int>(values.size()); ++i)
                    values[i] = i;

                std::mt19937 rng(std::random_device{}());
                std::shuffle(values.begin(), values.end(), rng);

                for (int y = 0; y < GRID_H; ++y)
                    for (int x = 0; x < GRID_W; ++x)
                        grid[y * GRID_W + x] = values[y * GRID_W + x];
            }
        };

        void setupSprites() {
            tiles.clear();

            auto tileImg = a_loadImage("assets/atestimage.ppm", false);
            if (tileImg.pixels.empty())
                throw std::runtime_error("[Sliding] Failed to load tile image");

            if (tileImg.width % GRID_W != 0 || tileImg.height % GRID_H != 0)
                throw std::runtime_error("[Sliding] Tile image dimensions must be divisible by grid size");

            const int tileWidth = tileImg.width / GRID_W;
            const int tileHeight = tileImg.height / GRID_H;

            const bool createdAtlas = atlasmanager::create_atlas({
                .name = "sliding_atlas",
                .width = 512,
                .height = 512,
                .generate_mipmaps = false });

            auto* registrar = atlasmanager::get_registrar("sliding_atlas");
            if (!registrar)
                throw std::runtime_error("[Sliding] Missing atlas registrar");

            TextureAtlas& atlas = registrar->atlas;
            bool registeredTile = false;

            for (int tileId = 1; tileId < GRID_W * GRID_H; ++tileId) {
                const std::string spriteName = "tile" + std::to_string(tileId);

                if (auto existing = atlasmanager::registry.get(spriteName)) {
                    auto handle = std::get<0>(*existing);
                    if (spritepool::is_alive(handle)) {
                        tiles[tileId] = handle;
                        continue;
                    }
                }

                std::vector<unsigned char> tilePixels(tileWidth * tileHeight * 4);

                const int tx = tileId % GRID_W;
                const int ty = tileId / GRID_W;

                for (int y = 0; y < tileHeight; ++y) {
                    const int srcY = ty * tileHeight + y;
                    for (int x = 0; x < tileWidth; ++x) {
                        const int srcX = tx * tileWidth + x;
                        const int srcIdx = (srcY * tileImg.width + srcX) * 4;
                        const int dstIdx = (y * tileWidth + x) * 4;
                        for (int c = 0; c < 4; ++c)
                            tilePixels[dstIdx + c] = tileImg.pixels[srcIdx + c];
                    }
                }

                auto handleOpt = registrar->register_atlas_sprites_by_image(
                    spriteName,
                    tilePixels,
                    tileWidth,
                    tileHeight,
                    atlas);

                if (!handleOpt || !spritepool::is_alive(*handleOpt))
                    throw std::runtime_error("[Sliding] Failed to register tile sprite: " + spriteName);

                tiles[tileId] = *handleOpt;
                registeredTile = true;
            }

            if (createdAtlas || registeredTile) {
                atlas.rebuild_pixels();
                atlasmanager::ensure_uploaded(atlas);
            }
        }

        State state{};
        std::unordered_map<int, SpriteHandle> tiles{};
        time::Timer timer{};
        double acc = 0.0;
        bool movedThisFrame = false;
    };

    inline bool run_sliding(std::shared_ptr<core::Context> ctx)
    {
        SlidingScene scene;
        scene.load();

        auto* window = ctx ? ctx->windowData : nullptr;
        bool running = true;

        while (running && ctx)
            running = scene.frame(ctx, window);

        scene.unload();
        return running;
    }
} // namespace almondnamespace::sliding
