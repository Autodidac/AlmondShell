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
 // atetrislike.hpp
#pragma once

#include "ascene.hpp"
#include "aplatformpump.hpp"
#include "arobusttime.hpp"
#include "acontext.hpp"
#include "ainput.hpp"
#include "agamecore.hpp"
#include "aatlasmanager.hpp"
#include "aopengltextures.hpp"
#include "aspritepool.hpp"

#include <array>
#include <random>
#include <stdexcept>
#include <span>

namespace almondnamespace::tetris 
{
    struct TetrisScene : public scene::Scene {
        TetrisScene(Logger* L = nullptr, time::Timer* C = nullptr)
            : Scene(L, C) {
        }

        void load() override {
            Scene::load();
            if (!setup_sprites())
                throw std::runtime_error("Failed to setup Tetris sprites");
            state = {};
            timer = time::createTimer(0.25);
            time::setScale(timer, 0.25);
            acc = 0.0;
            game_over = false;
        }

        bool frame(std::shared_ptr<core::Context> ctx, core::WindowData*) override {
            if (game_over) return false;

            platform::pump_events();

            if (ctx->is_key_down_safe(input::Key::Escape)) return false;

            // --- Input ---
            if (ctx->is_key_down_safe(input::Key::Left) &&
                !collides(state, state.px - 1, state.py, state.rot))
                --state.px;

            if (ctx->is_key_down_safe(input::Key::Right) &&
                !collides(state, state.px + 1, state.py, state.rot))
                ++state.px;

            if (ctx->is_key_down_safe(input::Key::Up))
                state.rot = (state.rot + 1) % 4;

            if (ctx->is_key_down_safe(input::Key::Down) &&
                !collides(state, state.px, state.py + 1, state.rot))
                ++state.py;

            // --- Timing ---
            advance(timer, 0.016);
            acc += time::elapsed(timer);
            reset(timer);

            while (acc >= STEP_S) {
                acc -= STEP_S;
                if (!collides(state, state.px, state.py + 1, state.rot)) {
                    ++state.py;
                }
                else {
                    place(state);
                    clear_lines(state);
                    state.shape = state.dist(state.rng);
                    state.rot = 0;
                    state.px = GRID_W / 2 - 2;
                    state.py = 0;
                    if (collides(state, state.px, state.py, state.rot)) {
                        game_over = true;
                        return false;
                    }
                }
            }

            // --- Draw ---
            ctx->clear_safe(ctx);
            draw(ctx);
            ctx->present_safe();
            return true;
        }

        void unload() override {
            Scene::unload();
        }

    private:
        // === Constants ===
        static constexpr int GRID_W = 40;
        static constexpr int GRID_H = 20;
        static constexpr double STEP_S = 30.0;

        // === State ===
        struct State {
            gamecore::grid_t<int> grid;
            int shape, rot;
            int px, py;
            std::mt19937_64 rng;
            std::uniform_int_distribution<int> dist;
            double accumulator = 0.0;

            State()
                : grid(gamecore::make_grid<int>(GRID_W, GRID_H, 0))
                , shape(0), rot(0), px(GRID_W / 2 - 2), py(0)
                , rng{ std::random_device{}() }
                , dist(0, 6) {
                shape = dist(rng);
            }
        } state;

        time::Timer timer{};
        double acc = 0.0;
        bool game_over = false;

        static inline SpriteRegistry registry;

        // === Tetromino definitions ===
        static constexpr std::array<std::array<std::array<int, 16>, 4>, 7> TETRAMINOS = { {
                // I
                {{{0,1,0,0,  0,1,0,0,  0,1,0,0,  0,1,0,0},
                  {0,0,0,0,  1,1,1,1,  0,0,0,0,  0,0,0,0},
                  {0,0,1,0,  0,0,1,0,  0,0,1,0,  0,0,1,0},
                  {0,0,0,0,  0,0,0,0,  1,1,1,1,  0,0,0,0}}},
                  // O
                  {{{0,0,0,0,  0,1,1,0,  0,1,1,0,  0,0,0,0},
                    {0,0,0,0,  0,1,1,0,  0,1,1,0,  0,0,0,0},
                    {0,0,0,0,  0,1,1,0,  0,1,1,0,  0,0,0,0},
                    {0,0,0,0,  0,1,1,0,  0,1,1,0,  0,0,0,0}}},
                    // T
                    {{{0,0,0,0,  1,1,1,0,  0,1,0,0,  0,0,0,0},
                      {0,0,1,0,  0,1,1,0,  0,0,1,0,  0,0,0,0},
                      {0,0,0,0,  0,1,0,0,  1,1,1,0,  0,0,0,0},
                      {0,1,0,0,  1,1,0,0,  0,1,0,0,  0,0,0,0}}},
                      // S
                      {{{0,0,0,0,  0,1,1,0,  1,1,0,0,  0,0,0,0},
                        {0,1,0,0,  0,1,1,0,  0,0,1,0,  0,0,0,0},
                        {0,0,0,0,  0,1,1,0,  1,1,0,0,  0,0,0,0},
                        {0,1,0,0,  0,1,1,0,  0,0,1,0,  0,0,0,0}}},
                        // Z
                        {{{0,0,0,0,  1,1,0,0,  0,1,1,0,  0,0,0,0},
                          {0,0,1,0,  0,1,1,0,  0,1,0,0,  0,0,0,0},
                          {0,0,0,0,  1,1,0,0,  0,1,1,0,  0,0,0,0},
                          {0,0,1,0,  0,1,1,0,  0,1,0,0,  0,0,0,0}}},
                          // J
                          {{{0,0,0,0,  1,0,0,0,  1,1,1,0,  0,0,0,0},
                            {0,0,1,1,  0,0,1,0,  0,0,1,0,  0,0,0,0},
                            {0,0,0,0,  1,1,1,0,  0,0,1,0,  0,0,0,0},
                            {0,0,1,0,  0,0,1,0,  0,1,1,0,  0,0,0,0}}},
                            // L
                            {{{0,0,0,0,  0,0,1,0,  1,1,1,0,  0,0,0,0},
                              {0,0,1,0,  0,0,1,0,  0,0,1,1,  0,0,0,0},
                              {0,0,0,0,  1,1,1,0,  1,0,0,0,  0,0,0,0},
                              {0,1,1,0,  0,0,1,0,  0,0,1,0,  0,0,0,0}}}
                        } };

        // === Helpers ===
        inline bool setup_sprites() {
            const auto blockImg = a_loadImage("assets/atestimage.ppm", true);
            if (blockImg.pixels.empty()) return false;

            Texture blockTex{
                .width = static_cast<u32>(blockImg.width),
                .height = static_cast<u32>(blockImg.height),
                .pixels = blockImg.pixels
            };

            if (!atlasmanager::create_atlas({
                .name = "tetrisatlas",
                .width = 1024,
                .height = 1024,
                .generate_mipmaps = false }))
                return false;

            auto& registrar = *atlasmanager::get_registrar("tetrisatlas");
            TextureAtlas& atlas = registrar.atlas;

            auto handleOpt = registrar.register_atlas_sprites_by_image(
                "tetris_block", blockTex.pixels, blockTex.width, blockTex.height, atlas);
            if (!handleOpt) return false;

            SpriteHandle handle = *handleOpt;
            if (!spritepool::is_alive(handle)) return false;

            atlas.rebuild_pixels();
            atlasmanager::ensure_uploaded(atlas);

            registry.add("tetris_block", handle, 0.f, 0.f, 1.f, 1.f, 0.f, 0.f);
            registry.set_atlas(&atlas);
            return true;
        }

        inline bool collides(const State& s, int nx, int ny, int r) {
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j) {
                    int v = TETRAMINOS[s.shape][r][i * 4 + j];
                    if (!v) continue;
                    int gx = nx + j;
                    int gy = ny + i;
                    if (!gamecore::in_bounds(GRID_W, GRID_H, gx, gy) ||
                        gamecore::at(s.grid, GRID_W, GRID_H, gx, gy)) return true;
                }
            return false;
        }

        inline void place(State& s) {
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j) {
                    if (TETRAMINOS[s.shape][s.rot][i * 4 + j]) {
                        int gx = s.px + j;
                        int gy = s.py + i;
                        if (gamecore::in_bounds(GRID_W, GRID_H, gx, gy))
                            gamecore::at(s.grid, GRID_W, GRID_H, gx, gy) = s.shape + 1;
                    }
                }
        }

        inline void clear_lines(State& s) {
            for (int y = GRID_H - 1; y >= 0; --y) {
                bool full = true;
                for (int x = 0; x < GRID_W; ++x) {
                    if (!gamecore::at(s.grid, GRID_W, GRID_H, x, y)) {
                        full = false;
                        break;
                    }
                }
                if (full) {
                    for (int ny = y; ny > 0; --ny)
                        for (int x = 0; x < GRID_W; ++x)
                            gamecore::at(s.grid, GRID_W, GRID_H, x, ny) =
                            gamecore::at(s.grid, GRID_W, GRID_H, x, ny - 1);

                    for (int x = 0; x < GRID_W; ++x)
                        gamecore::at(s.grid, GRID_W, GRID_H, x, 0) = 0;
                    ++y; // re-check after shifting
                }
            }
        }

        inline void draw(std::shared_ptr<core::Context> ctx) {
            float cw = float(ctx->get_width_safe()) / GRID_W;
            float ch = float(ctx->get_height_safe()) / GRID_H;

            auto entry = registry.get("tetris_block");
            if (!entry || !registry.get_atlas()) return;

            auto& [handle, u0, v0, u1, v1, px, py] = *entry;
            if (!spritepool::is_alive(handle)) return;

            auto& atlasVec = atlasmanager::get_atlas_vector();
            std::span<const TextureAtlas* const> atlasSpan(atlasVec.data(), atlasVec.size());

            // Placed blocks
            for (int y = 0; y < GRID_H; ++y) {
                float py = y * ch;
                for (int x = 0; x < GRID_W; ++x) {
                    if (gamecore::at(state.grid, GRID_W, GRID_H, x, y)) {
                        ctx->draw_sprite_safe(handle, atlasSpan, x * cw, py, cw, ch);
                    }
                }
            }

            // Falling tetromino
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    if (TETRAMINOS[state.shape][state.rot][i * 4 + j]) {
                        float dx = (state.px + j) * cw;
                        float dy = (state.py + i) * ch;
                        ctx->draw_sprite_safe(handle, atlasSpan, dx, dy, cw, ch);
                    }
                }
            }
        }
    };

} // namespace almondnamespace
