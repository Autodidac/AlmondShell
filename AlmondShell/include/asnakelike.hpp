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
 // asnakelike.hpp - functional Snake mini‑app
#pragma once

#include "ascene.hpp"
#include "aplatformpump.hpp"
#include "arobusttime.hpp"
#include "acontext.hpp"
#include "agamecore.hpp"
#include "aecs.hpp"
#include "aeventsystem.hpp"
#include "ainput.hpp"
#include "aatlasmanager.hpp"
#include "aopengltextures.hpp"

#include <deque>
#include <random>
#include <stdexcept>
#include <array>
#include <vector>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

namespace almondnamespace::snake {

    struct SnakeScene : public scene::Scene {
        SnakeScene(Logger* L = nullptr, time::Timer* C = nullptr)
            : Scene(L, C) {
        }

        void load() override {
            Scene::load();
            setupSprites();
            initGame();
        }

        bool frame(std::shared_ptr<core::Context> ctx, core::WindowData*) override {
            if (game_over) return false;

         //   input::poll_input();
            events::pump();

            if (ctx->is_key_held(input::Key::Escape)) return false;

            handleInput(ctx);
            tickTimers();
            updateSnake();

            auto& atlasVec = atlasmanager::get_atlas_vector();
            std::span<const TextureAtlas* const> atlasSpan(atlasVec.data(), atlasVec.size());

            float cw = float(ctx->get_width_safe()) / GRID_W;
            float ch = float(ctx->get_height_safe()) / GRID_H;

            ctx->clear_safe(ctx);

            // Body
            for (auto it = snakeSegments.begin(); it != snakeSegments.end(); ++it) {
                auto& p = ecs::get_component<Position>(world, *it);
                bool isHead = (std::next(it) == snakeSegments.end());
                if (!isHead)
                    ctx->draw_sprite_safe(bodyHandle, atlasSpan, p.x * cw, p.y * ch, cw, ch);
            }

            // Head
            {
                auto& p = ecs::get_component<Position>(world, snakeSegments.back());
                ctx->draw_sprite_safe(headHandle, atlasSpan, p.x * cw, p.y * ch, cw, ch);

                int tx = (p.x + dir.x + GRID_W) % GRID_W;
                int ty = (p.y + dir.y + GRID_H) % GRID_H;

                SpriteHandle tongueHandle = (dir.x == -1) ? tongueLeftHandle :
                    (dir.x == 1) ? tongueRightHandle :
                    (dir.y == -1) ? tongueUpHandle :
                    tongueDownHandle;

                if (tongueFrame == 1)
                    ctx->draw_sprite_safe(tongueHandle, atlasSpan,
                        tx * cw, ty * ch, cw, ch);
            }

            // Food
            {
                auto& p = ecs::get_component<Position>(world, food);
                ctx->draw_sprite_safe(foodHandle, atlasSpan, p.x * cw, p.y * ch, cw, ch);
            }

            ctx->present_safe();
            return !game_over;
        }

        void unload() override {
            Scene::unload();
            snakeSegments.clear();
        }

    private:
        // === Constants ===
        static constexpr int GRID_W = 40;
        static constexpr int GRID_H = 30;
        static constexpr double STEP_S = 3.0;
        static constexpr int tongueFrameCount = 2;
        static constexpr double tongueStep = 0.25;

        struct Position { int x, y; };
        using Entity = ecs::Entity;

        // === ECS world ===
        ecs::reg_ex<Position> world;
        std::deque<Entity> snakeSegments;
        Entity food{ 0 };

        // === Game state ===
        gamecore::grid_t<bool> occupied;
        Position dir{ -1, 0 };
        Position lastdir{ 0, 0 };

        time::Timer timer{};
        double acc = 0.0;
        double tongueTimer = 0.0;
        int tongueFrame = 0;
        bool game_over = false;

        // === Sprites ===
        SpriteHandle headHandle{};
        SpriteHandle bodyHandle{};
        SpriteHandle foodHandle{};
        SpriteHandle tongueUpHandle{};
        SpriteHandle tongueDownHandle{};
        SpriteHandle tongueLeftHandle{};
        SpriteHandle tongueRightHandle{};

        // === Helpers ===
        inline void setupSprites() {
            const auto headImg = a_loadImage("assets/snake/head.ppm", true);
            const auto bodyImg = a_loadImage("assets/snake/body.ppm", true);
            const auto foodImg = a_loadImage("assets/snake/food.ppm");
            const auto tongueUp = a_loadImage("assets/snake/tongue_up.ppm");
            const auto tongueDn = a_loadImage("assets/snake/tongue_down.ppm");
            const auto tongueLt = a_loadImage("assets/snake/tongue_left.ppm");
            const auto tongueRt = a_loadImage("assets/snake/tongue_right.ppm");

            if (headImg.pixels.empty() || bodyImg.pixels.empty() || foodImg.pixels.empty() ||
                tongueUp.pixels.empty() || tongueDn.pixels.empty() ||
                tongueLt.pixels.empty() || tongueRt.pixels.empty()) {
                throw std::runtime_error("Failed to load snake textures");
            }

            const bool createdAtlas = atlasmanager::create_atlas({
                .name = "snakeatlas",
                .width = 1024,
                .height = 1024,
                .generate_mipmaps = false });

            auto* registrarPtr = atlasmanager::get_registrar("snakeatlas");
            if (!registrarPtr) {
                throw std::runtime_error("Failed to get snake atlas registrar");
            }

            TextureAtlas& atlas = registrarPtr->atlas;
            bool registeredAny = false;

            auto ensureSprite = [&](std::string_view name, const ImageData& img, SpriteHandle& handle) {
                if (auto existing = atlasmanager::registry.get(std::string(name))) {
                    auto candidate = std::get<0>(*existing);
                    if (spritepool::is_alive(candidate)) {
                        handle = candidate;
                        return;
                    }
                }

                auto h = registrarPtr->register_atlas_sprites_by_image(std::string(name), img.pixels, img.width, img.height, atlas);
                if (!h || !spritepool::is_alive(*h)) {
                    throw std::runtime_error("Invalid sprite handle for '" + std::string(name) + "'");
                }

                handle = *h;
                registeredAny = true;
            };

            ensureSprite("snake_head", headImg, headHandle);
            ensureSprite("snake_body", bodyImg, bodyHandle);
            ensureSprite("snake_food", foodImg, foodHandle);
            ensureSprite("snake_tongue_up", tongueUp, tongueUpHandle);
            ensureSprite("snake_tongue_down", tongueDn, tongueDownHandle);
            ensureSprite("snake_tongue_left", tongueLt, tongueLeftHandle);
            ensureSprite("snake_tongue_right", tongueRt, tongueRightHandle);

            if (createdAtlas || registeredAny) {
                atlas.rebuild_pixels();
                atlasmanager::ensure_uploaded(atlas);
            }
        }

        inline void initGame() {
            world = ecs::make_registry<Position>();
            occupied = gamecore::make_grid<bool>(GRID_W, GRID_H, false);
            snakeSegments.clear();

            auto spawn = [&](int gx, int gy) {
                auto e = ecs::create_entity(world);
                ecs::add_component(world, e, Position{ gx, gy });
                snakeSegments.push_back(e);
                gamecore::at(occupied, GRID_W, GRID_H, gx, gy) = true;
                };

            spawn(GRID_W / 2, GRID_H / 2);

            food = ecs::create_entity(world);
            ecs::add_component(world, food, Position{ 0, 0 });
            place_food();

            timer = time::createTimer(0.25);
            time::setScale(timer, 0.25);
            acc = 0.0;
            tongueTimer = 0.0;
            tongueFrame = 0;
            game_over = false;
            dir = { -1, 0 };
            lastdir = { 0, 0 };
        }

        inline void handleInput(std::shared_ptr<core::Context> ctx) {
            if ((ctx->is_key_held(input::Key::A) || ctx->is_key_down(input::Key::Left)) && lastdir.x != 1)  dir = { -1, 0 };
            if ((ctx->is_key_down(input::Key::D) || ctx->is_key_down(input::Key::Right)) && lastdir.x != -1) dir = { 1, 0 };
            if ((ctx->is_key_down(input::Key::W) || ctx->is_key_down(input::Key::Up)) && lastdir.y != 1)  dir = { 0, -1 };
            if ((ctx->is_key_down(input::Key::S) || ctx->is_key_down(input::Key::Down)) && lastdir.y != -1) dir = { 0, 1 };
            lastdir = dir;
        }

        inline void tickTimers() {
            advance(timer, 0.016);
            acc += time::elapsed(timer);
            tongueTimer += time::elapsed(timer);
            reset(timer);

            if (tongueTimer >= tongueStep) {
                tongueTimer -= tongueStep;
                tongueFrame = (tongueFrame + 1) % tongueFrameCount;
            }
        }

        inline void updateSnake() {
            if (acc < STEP_S) return;   // not enough time elapsed yet
            acc -= STEP_S;

            auto headE = snakeSegments.back();
            auto headP = ecs::get_component<Position>(world, headE);

            int nx = (headP.x + dir.x + GRID_W) % GRID_W;
            int ny = (headP.y + dir.y + GRID_H) % GRID_H;

            if (gamecore::at(occupied, GRID_W, GRID_H, nx, ny)) {
                game_over = true;
                return;
            }

            auto e = ecs::create_entity(world);
            ecs::add_component(world, e, Position{ nx, ny });
            snakeSegments.push_back(e);
            gamecore::at(occupied, GRID_W, GRID_H, nx, ny) = true;

            auto& fp = ecs::get_component<Position>(world, food);
            if (nx == fp.x && ny == fp.y) {
                place_food();
            }
            else {
                auto tailE = snakeSegments.front();
                auto tailP = ecs::get_component<Position>(world, tailE);
                gamecore::at(occupied, GRID_W, GRID_H, tailP.x, tailP.y) = false;
                ecs::destroy_entity(world, tailE);
                snakeSegments.pop_front();
            }
        }


        inline void place_food() {
            std::mt19937_64 rng{ std::random_device{}() };
            std::uniform_int_distribution<int> dX(0, GRID_W - 1), dY(0, GRID_H - 1);
            int fx, fy;
            do { fx = dX(rng); fy = dY(rng); } while (gamecore::at(occupied, GRID_W, GRID_H, fx, fy));
            ecs::get_component<Position>(world, food) = { fx, fy };
        }
    };

} // namespace almondnamespace::snake
