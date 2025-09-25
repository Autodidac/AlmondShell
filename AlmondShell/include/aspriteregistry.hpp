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
 // aspriteregistry.hpp
#pragma once

#include "aplatform.hpp"
#include "aspritehandle.hpp"
#include "aspritepool.hpp"

#include <atomic>
#include <iostream>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>

namespace almondnamespace {

    struct TextureAtlas; // Forward declaration, implementation elsewhere

    /// Transparent hash + equal_to for std::string and std::string_view
    struct TransparentHash {
        using is_transparent = void;

        size_t operator()(std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }
        size_t operator()(const std::string& s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
    };

    struct TransparentEqual {
        using is_transparent = void;

        bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
            return lhs == rhs;
        }
        bool operator()(const std::string& lhs, std::string_view rhs) const noexcept {
            return lhs == rhs;
        }
        bool operator()(std::string_view lhs, const std::string& rhs) const noexcept {
            return lhs == rhs;
        }
        bool operator()(const std::string& lhs, const std::string& rhs) const noexcept {
            return lhs == rhs;
        }
    };

    /// Maps sprite names → (SpriteHandle, u0, v0, u1, v1, pivotX, pivotY)
    struct SpriteRegistry {
        using Entry = std::tuple<SpriteHandle, float, float, float, float, float, float>;

        std::unordered_map<
            std::string,
            Entry,
            TransparentHash,
            TransparentEqual
        > sprites;

        mutable std::shared_mutex mutex;
        std::atomic<const TextureAtlas*> atlas_ptr{ nullptr };

        void add(std::string_view name, SpriteHandle handle, float u0, float v0, float width, float height, float pivotX = 0.f, float pivotY = 0.f) {
            if (!handle.is_valid() || !spritepool::is_alive(handle)) {
                std::cerr << "[SpriteRegistry] Rejecting invalid handle for sprite '" << std::string{ name } << "'\n";
                return;
            }

            // Check if the handle is already used for a different sprite to avoid duplicates
            std::unique_lock lock(mutex);

            for (const auto& [existingName, entry] : sprites) {
                const SpriteHandle& existingHandle = std::get<0>(entry);
                if (existingHandle == handle && existingName != name) {
                    std::cerr << "[SpriteRegistry] Duplicate handle detected for sprite '" << std::string{ name } << "'\n";
                    return;
                }
            }

            float u1 = u0 + width;
            float v1 = v0 + height;
            sprites.emplace(std::string{ name }, Entry{ handle, u0, v0, u1, v1, pivotX, pivotY });

#if defined(DEBUG_TEXTURE_RENDERING_VERBOSE)
            std::cout << "[SpriteRegistry] Added: '" << name << "' handle=" << handle.id << " UV=("
                << u0 << "," << v0 << ")->(" << u1 << "," << v1 << ")\n";
#endif
        }

        [[nodiscard]]
        std::optional<Entry> get(std::string_view name) const noexcept {
            std::shared_lock lock(mutex);
            auto it = sprites.find(name); // Uses transparent lookup
            return (it != sprites.end()) ? std::optional{ it->second } : std::nullopt;
        }

        bool remove(std::string_view name) {
            std::unique_lock lock(mutex);
            auto it = sprites.find(name);
            if (it != sprites.end()) {
                sprites.erase(it);
                return true;
            }
            return false;
        }

        bool remove_if_invalid(std::string_view name) {
            std::unique_lock lock(mutex);
            auto it = sprites.find(name);
            if (it == sprites.end())
                return false;

            const SpriteHandle handle = std::get<0>(it->second);
            if (!spritepool::is_alive(handle)) {
                sprites.erase(it);
                return true;
            }
            return false;
        }

        void cleanup_dead() {
            std::unique_lock lock(mutex);
            for (auto it = sprites.begin(); it != sprites.end();) {
                if (!spritepool::is_alive(std::get<0>(it->second)))
                    it = sprites.erase(it);
                else
                    ++it;
            }
        }

        void clear() noexcept {
            std::unique_lock lock(mutex);
            sprites.clear();
        }

        void set_atlas(const TextureAtlas* atlas) noexcept {
            atlas_ptr.store(atlas, std::memory_order_release);
        }

        [[nodiscard]] const TextureAtlas* get_atlas() const noexcept {
            return atlas_ptr.load(std::memory_order_acquire);
        }
    };

} // namespace almondnamespace

