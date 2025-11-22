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
// ascene.hpp
#pragma once

#include "aplatform.hpp"       // must always come first
#include "aentitycomponents.hpp" // e.g. Position
#include "aecs.hpp"            // ECS registry
#include "amovementevent.hpp"  // MovementEvent
#include "arobusttime.hpp"     // Timer
#include "acontext.hpp"        // Context
#include "awindowdata.hpp"     // WindowData
#include "alogger.hpp"         // Logger

#include <memory>
#include <iostream>

namespace almondnamespace::scene
{
    class Scene {
    public:
        using Registry = almondnamespace::ecs::reg_ex</* component types go here */>;

        Scene(Logger* L = nullptr,
            time::Timer* C = nullptr,
            LogLevel sceneLevel = LogLevel::INFO)
            : reg(ecs::make_registry</* component types */>(nullptr, nullptr)), // ECS stays silent
            logger(L),
            clock(C),
            sceneLogLevel(sceneLevel)
        {
        }

        // Non-copyable, but movable
        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
        Scene(Scene&&) noexcept = default;
        Scene& operator=(Scene&&) noexcept = default;

        virtual ~Scene() = default;

        // Lifecycle
        virtual void load() {
            log("[Scene] Loaded", LogLevel::INFO);
            loaded = true;
        }

        virtual void unload() {
            log("[Scene] Unloaded", LogLevel::INFO);
            loaded = false;
            reg = ecs::make_registry</* component types */>(nullptr, nullptr);
        }

        // Per-frame hook (override in derived game scenes)
        virtual bool frame(std::shared_ptr<almondnamespace::core::Context>,
            almondnamespace::core::WindowData*) {
            return true; // default: no-op
        }

        // Entity management
        ecs::Entity createEntity() {
            ecs::Entity e = ecs::create_entity(reg);
            log("[Scene] Created entity " + std::to_string(e), LogLevel::INFO);
            return e;
        }

        void destroyEntity(ecs::Entity e) {
            ecs::destroy_entity(reg, e);
            log("[Scene] Destroyed entity " + std::to_string(e), LogLevel::INFO);
        }

        // Apply external event
        void applyMovementEvent(const MovementEvent& ev) {
            if (ecs::has_component<ecs::Position>(reg, ev.getEntityId())) {
                auto& pos = ecs::get_component<ecs::Position>(reg, ev.getEntityId());
                pos.x += ev.getDeltaX();
                pos.y += ev.getDeltaY();
                log("[Scene] Moved entity " + std::to_string(ev.getEntityId()) +
                    " by (" + std::to_string(ev.getDeltaX()) + "," +
                    std::to_string(ev.getDeltaY()) + ")", LogLevel::INFO);
            }
        }

        // Clone
        virtual std::unique_ptr<Scene> clone() const {
            auto newScene = std::make_unique<Scene>(logger, clock, sceneLogLevel);
            log("[Scene] Cloned scene", LogLevel::INFO);
            // TODO: deep copy ECS registry components
            return newScene;
        }

        bool isLoaded() const { return loaded; }

        Registry& registry() { return reg; }
        const Registry& registry() const { return reg; }

        void setLogLevel(LogLevel lvl) { sceneLogLevel = lvl; }
        LogLevel getLogLevel() const { return sceneLogLevel; }

    protected:
        void log(const std::string& msg, LogLevel lvl) const {
            if (logger && lvl >= sceneLogLevel) {
                logger->log(msg, lvl);
            }
        }

    private:
        Registry reg;
        bool loaded = false;
        Logger* logger = nullptr;       // optional shared logger
        time::Timer* clock = nullptr;   // optional time reference
        LogLevel sceneLogLevel;         // per-scene verbosity threshold
    };

} // namespace almondnamespace
