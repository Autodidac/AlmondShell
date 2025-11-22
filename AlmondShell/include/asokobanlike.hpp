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
 // asokobangame.hpp
#pragma once

#include "acontext.hpp"
#include "aplatformpump.hpp"
#include "arobusttime.hpp"
#include "ainput.hpp"
#include "agamecore.hpp"
#include "aatlasmanager.hpp"
#include "aspritepool.hpp"
#include "ascene.hpp"
#include "aimageloader.hpp"

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <stdexcept>

namespace almondnamespace::sokoban
{
    constexpr int GRID_W = 16;
    constexpr int GRID_H = 12;
    enum Cell { FLOOR, WALL, GOAL };

    struct State
    {
        almondnamespace::gamecore::grid_t<int> map;
        almondnamespace::gamecore::grid_t<bool> box;
        int px, py;

        State()
            : map(almondnamespace::gamecore::make_grid<int>(GRID_W, GRID_H, FLOOR)),
            box(almondnamespace::gamecore::make_grid<bool>(GRID_W, GRID_H, false)),
            px(1), py(1)
        {
            // Walls around edges
            for (int x = 0; x < GRID_W; ++x) {
                map[x + 0 * GRID_W] = WALL;
                map[x + (GRID_H - 1) * GRID_W] = WALL;
            }
            for (int y = 0; y < GRID_H; ++y) {
                map[0 + y * GRID_W] = WALL;
                map[(GRID_W - 1) + y * GRID_W] = WALL;
            }

            // Sample goal and box positions
            map[7 + 5 * GRID_W] = GOAL;
            box[6 + 5 * GRID_W] = true;

            px = 4;
            py = 5;
        }
    };

    struct SokobanScene : public scene::Scene {
        SokobanScene(Logger* L = nullptr, time::Timer* C = nullptr)
            : Scene(L, C)
        {
        }

        void load() override {
            Scene::load();
            setupSprites();
            state = {};
        }

        bool frame(std::shared_ptr<core::Context> ctx, core::WindowData*) override {
            if (!ctx)
                return false;

            platform::pump_events();
            if (ctx->is_key_down_safe(input::Key::Escape))
                return false;

            int dx = 0;
            int dy = 0;
            if (ctx->is_key_down_safe(input::Key::A) || ctx->is_key_down_safe(input::Key::Left))  dx = -1;
            if (ctx->is_key_down_safe(input::Key::D) || ctx->is_key_down_safe(input::Key::Right)) dx = 1;
            if (ctx->is_key_down_safe(input::Key::W) || ctx->is_key_down_safe(input::Key::Up))    dy = -1;
            if (ctx->is_key_down_safe(input::Key::S) || ctx->is_key_down_safe(input::Key::Down))  dy = 1;

            int nx = state.px + dx;
            int ny = state.py + dy;

            bool blocked = false;
            if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) blocked = true;
            else if (state.map[nx + ny * GRID_W] == WALL) blocked = true;

            if (!blocked && state.box[nx + ny * GRID_W]) {
                int bx = nx + dx;
                int by = ny + dy;

                bool boxBlocked = (bx < 0 || bx >= GRID_W || by < 0 || by >= GRID_H) ||
                    (state.map[bx + by * GRID_W] == WALL) ||
                    state.box[bx + by * GRID_W];

                if (!boxBlocked) {
                    state.box[bx + by * GRID_W] = true;
                    state.box[nx + ny * GRID_W] = false;
                }
                else {
                    blocked = true;
                }
            }

            if (!blocked) {
                state.px = nx;
                state.py = ny;
            }

            ctx->clear_safe(ctx);

            auto& atlasVec = atlasmanager::get_atlas_vector();
            std::span<const TextureAtlas* const> atlasSpan(atlasVec.data(), atlasVec.size());

            const float cw = float((std::max)(1, ctx->get_width_safe())) / GRID_W;
            const float ch = float((std::max)(1, ctx->get_height_safe())) / GRID_H;

            for (int y = 0; y < GRID_H; ++y) {
                for (int x = 0; x < GRID_W; ++x) {
                    SpriteHandle handle = floorHandle;
                    switch (state.map[x + y * GRID_W]) {
                    case FLOOR: handle = floorHandle; break;
                    case WALL:  handle = wallHandle;  break;
                    case GOAL:  handle = goalHandle;  break;
                    default:    handle = floorHandle; break;
                    }

                    if (spritepool::is_alive(handle))
                        ctx->draw_sprite_safe(handle, atlasSpan, x * cw, y * ch, cw, ch);

                    if (state.box[x + y * GRID_W] && spritepool::is_alive(boxHandle))
                        ctx->draw_sprite_safe(boxHandle, atlasSpan, x * cw, y * ch, cw, ch);
                }
            }

            if (spritepool::is_alive(playerHandle))
                ctx->draw_sprite_safe(playerHandle, atlasSpan, state.px * cw, state.py * ch, cw, ch);

            ctx->present_safe();
            return true;
        }

        void unload() override {
            Scene::unload();
            state = {};
        }

    private:
        void setupSprites() {
            const bool createdAtlas = atlasmanager::create_atlas({
                .name = "sokoban_atlas",
                .width = 512,
                .height = 512,
                .generate_mipmaps = false });

            auto* registrar = atlasmanager::get_registrar("sokoban_atlas");
            if (!registrar)
                throw std::runtime_error("[Sokoban] Failed to get atlas registrar");

            TextureAtlas& atlas = registrar->atlas;
            bool registeredAny = false;

            auto ensureSprite = [&](const char* name, const char* path, SpriteHandle& out) {
                if (auto existing = atlasmanager::registry.get(std::string(name))) {
                    auto handle = std::get<0>(*existing);
                    if (spritepool::is_alive(handle)) {
                        out = handle;
                        return;
                    }
                }

                auto img = a_loadImage(std::string(path), false);
                if (img.pixels.empty())
                    throw std::runtime_error("[Sokoban] Failed to load sprite asset: " + std::string(path));

                auto handleOpt = registrar->register_atlas_sprites_by_image(
                    std::string(name), img.pixels, img.width, img.height, atlas);
                if (!handleOpt || !spritepool::is_alive(*handleOpt))
                    throw std::runtime_error("[Sokoban] Failed to register sprite: " + std::string(name));

                out = *handleOpt;
                registeredAny = true;
            };

            ensureSprite("wall", "assets/atestimage.ppm", wallHandle);
            ensureSprite("floor", "assets/defaults/default.ppm", floorHandle);
            ensureSprite("goal", "assets/atestimage.ppm", goalHandle);
            ensureSprite("box", "assets/atestimage.ppm", boxHandle);
            ensureSprite("player", "assets/defaults/yellow.ppm", playerHandle);

            if (createdAtlas || registeredAny) {
                atlas.rebuild_pixels();
                atlasmanager::ensure_uploaded(atlas);
            }
        }

        State state{};
        SpriteHandle wallHandle{};
        SpriteHandle floorHandle{};
        SpriteHandle goalHandle{};
        SpriteHandle boxHandle{};
        SpriteHandle playerHandle{};
    };

    inline bool run_sokoban(std::shared_ptr<core::Context> ctx)
    {
        SokobanScene scene;
        scene.load();

        auto* window = ctx ? ctx->windowData : nullptr;
        bool running = true;
        while (running && ctx)
            running = scene.frame(ctx, window);

        scene.unload();
        return running;
    }
} // namespace almondnamespace::sokoban
