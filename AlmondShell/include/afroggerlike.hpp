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
 // afroggerlike.hpp
#pragma once

#include "aplatformpump.hpp"
#include "arobusttime.hpp"
#include "acontext.hpp"
#include "ainput.hpp"
#include "agamecore.hpp"
#include "aatlasmanager.hpp"
#include "aspritepool.hpp"
#include "ascene.hpp"
#include "aimageloader.hpp"

#include <vector>
#include <chrono>
#include <unordered_map>
#include <string>
#include <iostream>
#include <stdexcept>

namespace almondnamespace::frogger {

    constexpr int GRID_W = 16;
    constexpr int GRID_H = 12;

    struct GameState {
        int frogX, frogY;
        GameState() : frogX(GRID_W / 2), frogY(GRID_H - 1) {}
    };

    struct FroggerScene : public scene::Scene {
        FroggerScene(Logger* L = nullptr, time::Timer* C = nullptr)
            : Scene(L, C)
        {
        }

        void load() override {
            Scene::load();
            setupSprites();
            state = {};
            reachedGoal = false;
        }

        bool frame(std::shared_ptr<core::Context> ctx, core::WindowData*) override {
            if (!ctx)
                return false;

            platform::pump_events();
            if (ctx->is_key_down_safe(input::Key::Escape))
                return false;

            if (ctx->is_key_down_safe(input::Key::Left) && state.frogX > 0)         --state.frogX;
            if (ctx->is_key_down_safe(input::Key::Right) && state.frogX < GRID_W - 1)  ++state.frogX;
            if (ctx->is_key_down_safe(input::Key::Up) && state.frogY > 0)           --state.frogY;
            if (ctx->is_key_down_safe(input::Key::Down) && state.frogY < GRID_H - 1) ++state.frogY;

            if (state.frogY == 0)
                reachedGoal = true;

            ctx->clear_safe(ctx);

            auto& atlasVec = atlasmanager::get_atlas_vector();
            std::span<const TextureAtlas* const> atlasSpan(atlasVec.data(), atlasVec.size());

            const float cw = float(ctx->get_width_safe()) / GRID_W;
            const float ch = float(ctx->get_height_safe()) / GRID_H;

            if (spritepool::is_alive(frogHandle))
                ctx->draw_sprite_safe(frogHandle, atlasSpan,
                    state.frogX * cw, state.frogY * ch, cw, ch);

            ctx->present_safe();
            return !reachedGoal;
        }

        void unload() override {
            Scene::unload();
            state = {};
            reachedGoal = false;
        }

    private:
        void setupSprites() {
            const bool createdAtlas = atlasmanager::create_atlas({
                .name = "frogger_atlas",
                .width = 512,
                .height = 512,
                .generate_mipmaps = false });

            auto* registrar = atlasmanager::get_registrar("frogger_atlas");
            if (!registrar)
                throw std::runtime_error("[Frogger] Missing atlas registrar");

            TextureAtlas& atlas = registrar->atlas;
            bool registered = false;

            if (auto existing = atlasmanager::registry.get("frog")) {
                auto handle = std::get<0>(*existing);
                if (spritepool::is_alive(handle))
                    frogHandle = handle;
            }

            if (!spritepool::is_alive(frogHandle)) {
                auto frogImg = a_loadImage("assets/frog.ppm", false);
                if (frogImg.pixels.empty())
                    throw std::runtime_error("[Frogger] Failed to load frog.ppm");

                auto handleOpt = registrar->register_atlas_sprites_by_image(
                    "frog", frogImg.pixels, frogImg.width, frogImg.height, atlas);
                if (!handleOpt || !spritepool::is_alive(*handleOpt))
                    throw std::runtime_error("[Frogger] Failed to register frog sprite");

                frogHandle = *handleOpt;
                registered = true;
            }

            if (createdAtlas || registered) {
                atlas.rebuild_pixels();
                atlasmanager::ensure_uploaded(atlas);
            }
        }

        GameState state{};
        SpriteHandle frogHandle{};
        bool reachedGoal = false;
    };

    inline bool run_frogger(std::shared_ptr<core::Context> ctx) {
        FroggerScene scene;
        scene.load();

        auto* window = ctx ? ctx->windowData : nullptr;
        bool running = true;
        while (running && ctx)
            running = scene.frame(ctx, window);

        scene.unload();
        return running;
    }
    

} // namespace almondnamespace::frogger
