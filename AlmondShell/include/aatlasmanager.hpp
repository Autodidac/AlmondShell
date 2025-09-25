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
// aatlasmanager.hpp
#pragma once

#include "aspritepool.hpp"
#include "aatlastexture.hpp"
#include "aspriteregistry.hpp"
#include "aspritehandle.hpp"
#include "acontexttype.hpp"

#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace almondnamespace::atlasmanager
{
    using almondnamespace::spritepool::SpriteHandle;
    using almondnamespace::spritepool::allocate;

    using almondnamespace::TextureAtlas;

    inline SpriteRegistry registry;
    inline std::unordered_map<std::string, TextureAtlas> atlas_map;

    inline std::shared_mutex atlasMutex;
    inline std::shared_mutex registrarMutex;

    inline std::atomic<int> nextAtlasIndex = 0; // unique atlas id allocator

    // --- Global atlas vector for direct atlasIndex lookup ---
    inline std::vector<const TextureAtlas*> atlas_vector;

    struct AtlasRegistrar
    {
        TextureAtlas& atlas;  // Reference to the shared atlas

        explicit AtlasRegistrar(TextureAtlas& atlas_) noexcept
            : atlas(atlas_) {
        }


		// Bulk registration from slices (name, x, y, w, h) when loading from a texture atlas file
        bool register_atlas_sprites_by_custom_sizes(const std::vector<std::tuple<std::string, int, int, int, int>>& sliceRects)
        {
            for (const auto& [name, x, y, w, h] : sliceRects) {
                SpriteHandle handle = allocate();
                if (!handle.is_valid()) {
                    std::cerr << "[AtlasRegistrar] Failed to allocate handle for '" << name << "'\n";
                    return false;
                }

                auto added = atlas.add_slice_entry(name, x, y, w, h);
                if (!added) {
                    std::cerr << "[AtlasRegistrar] Failed to slice '" << name << "' from atlas\n";
                    return false;
                }

                handle.atlasIndex = atlas.get_index();
                handle.localIndex = static_cast<uint32_t>(added->index);

                registry.add(name, handle,
                    added->region.u1,
                    added->region.v1,
                    added->region.u2 - added->region.u1,
                    added->region.v2 - added->region.v1);
            }
            return true;
        }

        //// Bulk registration from slices (name, x, y, w, h)
//bool register_atlas_sprites_by_columns_and_rows(const std::vector<std::tuple<std::string, int, int, int, int>>& sliceRects) {
//    for (const auto& [name, x, y, w, h] : sliceRects) {
//        SpriteHandle handle = allocate();
//        if (!handle.is_valid()) {
//            std::cerr << "[AtlasRegistrar] Failed to allocate handle for '" << name << "'\n";
//            return false;
//        }

//        // Create dummy white texture data (RGBA)
//        Texture dummy
//        {
//            0, // id placeholder
//            name,
//            static_cast<u32>(w),
//            static_cast<u32>(h),
//            4, // channels RGBA
//            std::vector<u8>(size_t(w) * size_t(h) * 4, 255u)
//        };

//        auto added = atlas.add_entry(name, dummy);
//        if (!added) {
//            std::cerr << "[AtlasRegistrar] Failed to add '" << name << "' to atlas\n";
//            return false;
//        }

//        // Now, sync handle's atlasIndex and id to the actual stored sprite indices
//        // Assuming 'added' has an 'index' member for sprite index in atlas
//        handle.atlasIndex = atlas.get_index();  // or whatever your atlas index identifier is
//        handle.id = added->index;

//        // Register sprite with normalized UVs (width/height in UV space)
//        registry.add(name, handle,
//            added->region.u1,
//            added->region.v1,
//            added->region.u2 - added->region.u1,
//            added->region.v2 - added->region.v1);
//    }
//    return true;
//}

//// Bulk registration from slices (name, x, y, w, h)
//bool register_atlas_sprites_by_tile_size(const std::vector<std::tuple<std::string, int, int, int, int>>& sliceRects) {
//    for (const auto& [name, x, y, w, h] : sliceRects) {
//        SpriteHandle handle = allocate();
//        if (!handle.is_valid()) {
//            std::cerr << "[AtlasRegistrar] Failed to allocate handle for '" << name << "'\n";
//            return false;
//        }

//        // Create dummy white texture data (RGBA)
//        Texture dummy
//        {
//            0, // id placeholder
//            name,
//            static_cast<u32>(w),
//            static_cast<u32>(h),
//            4, // channels RGBA
//            std::vector<u8>(size_t(w) * size_t(h) * 4, 255u)
//        };

//        auto added = atlas.add_entry(name, dummy);
//        if (!added) {
//            std::cerr << "[AtlasRegistrar] Failed to add '" << name << "' to atlas\n";
//            return false;
//        }

//        // Now, sync handle's atlasIndex and id to the actual stored sprite indices
//        // Assuming 'added' has an 'index' member for sprite index in atlas
//        handle.atlasIndex = atlas.get_index();  // or whatever your atlas index identifier is
//        handle.id = added->index;

//        // Register sprite with normalized UVs (width/height in UV space)
//        registry.add(name, handle,
//            added->region.u1,
//            added->region.v1,
//            added->region.u2 - added->region.u1,
//            added->region.v2 - added->region.v1);
//    }
//    return true;
//}

        // Single sprite registration into an automatically constructed shared atlas
        std::optional<SpriteHandle> register_atlas_sprites_by_image(const std::string& name, const std::vector<u8>& pixels, u32 width, u32 height, TextureAtlas& sharedAtlas)
        {
            //std::cerr << "SharedAtlas index: " << sharedAtlas.index << "\n";
            //std::cerr << "AtlasMap[name] index: " << atlas_map[name].index << "\n";

            if (auto existing = registry.get(name))
                return std::get<0>(*existing);

            Texture tex{ 0, name, width, height, 4, pixels };
            auto addedOpt = sharedAtlas.add_entry(name, tex);
            if (!addedOpt) {
                std::cerr << "[AtlasRegistrar] Failed to add '" << name << "' to atlas\n";
                return std::nullopt;
            }
            auto& added = *addedOpt;
            const int localIndex = added.index;

            auto allocated = allocate();
            if (!allocated.is_valid()) {
                std::cerr << "[AtlasRegistrar] Failed to allocate spritepool handle for '" << name << "'\n";
                return std::nullopt;
            }

            SpriteHandle handle{
                allocated.id,
                allocated.generation,
                static_cast<uint32_t>(sharedAtlas.index),
                static_cast<uint32_t>(localIndex)
            };

            registry.add(name, handle,
                added.region.u1,
                added.region.v1,
                added.region.u2 - added.region.u1,
                added.region.v2 - added.region.v1);

            return handle;
        }
    };

    inline std::unordered_map<std::string, std::unique_ptr<AtlasRegistrar>> registrar_map;

    inline void update_atlas_vector_locked()
    {
        int maxIndex = -1;
        for (const auto& [name, atlas] : atlas_map) {
            std::cerr << "[update_atlas_vector] Atlas '" << name << "' index: " << atlas.index << "\n";
            if (atlas.index > maxIndex)
                maxIndex = atlas.index;
        }

        if (maxIndex >= 0)
        {
            if (atlas_vector.size() <= static_cast<size_t>(maxIndex))
                atlas_vector.resize(static_cast<size_t>(maxIndex) + 1, nullptr);

            for (const auto& [name, atlas] : atlas_map) {
                atlas_vector[atlas.index] = &atlas;
                std::cerr << "[update_atlas_vector] atlas_vector[" << atlas.index << "] assigned for '" << name << "'\n";
            }
        }
    }

    namespace detail
    {
        struct PendingUpload
        {
            const TextureAtlas* atlas{ nullptr };
            u64 version{ 0 };
        };

        struct BackendUploadState
        {
            std::function<void(const TextureAtlas&)> ensureFn{};
            std::queue<const TextureAtlas*> pending{};
            std::unordered_map<const TextureAtlas*, u64> uploadedVersions{};
            std::unordered_map<const TextureAtlas*, u64> pendingVersions{};
        };

        inline std::mutex backendMutex;
        inline std::unordered_map<core::ContextType, BackendUploadState> backendStates;
        inline thread_local bool processingUploads = false;
        inline thread_local std::optional<core::ContextType> activeBackend = std::nullopt;

        inline void enqueue_locked(BackendUploadState& state, const TextureAtlas& atlas)
        {
            const u64 version = atlas.version;
            auto [it, inserted] = state.pendingVersions.emplace(&atlas, version);
            if (!inserted && it->second >= version)
                return;

            it->second = version;
            state.pending.push(&atlas);
        }
    } // namespace detail

    inline void notify_backends_of_new_atlas(const TextureAtlas& atlas)
    {
        std::scoped_lock lock(detail::backendMutex);
        for (auto& [_, state] : detail::backendStates) {
            if (!state.ensureFn)
                continue;
            detail::enqueue_locked(state, atlas);
        }
    }

    inline bool create_atlas(const AtlasConfig& config)
    {
        AtlasConfig copy = config;
        copy.index = nextAtlasIndex.fetch_add(1, std::memory_order_relaxed);  // Assign unique incremental index
        TextureAtlas atlas = TextureAtlas::create(copy);

        if (atlas.index < 0) {
            std::cerr << "[create_atlas] Failed to initialize atlas '" << config.name << "'\n";
            return false;
        }

        std::unique_lock atlasLock(atlasMutex);
        auto [it, inserted] = atlas_map.emplace(config.name, std::move(atlas));
        if (!inserted) {
            std::cerr << "[create_atlas] Atlas already exists: " << config.name << "\n";
            return false;
        }

        TextureAtlas& storedAtlas = it->second;
        update_atlas_vector_locked();
        atlasLock.unlock();

        {
            std::unique_lock registrarLock(registrarMutex);
            registrar_map.emplace(config.name, std::make_unique<AtlasRegistrar>(storedAtlas));
        }

        notify_backends_of_new_atlas(storedAtlas);
        return true;
    }

    inline AtlasRegistrar* get_registrar(const std::string& name)
    {
        std::shared_lock lock(registrarMutex);
        auto it = registrar_map.find(name);
        return (it != registrar_map.end()) ? it->second.get() : nullptr;
    }

    inline const std::vector<const TextureAtlas*>& get_atlas_vector()
    {
        std::shared_lock lock(atlasMutex);
        return atlas_vector;
    }

    inline void register_backend_uploader(core::ContextType type,
        std::function<void(const TextureAtlas&)> ensureFn)
    {
        std::unique_lock backendLock(detail::backendMutex);
        auto& state = detail::backendStates[type];
        state.ensureFn = std::move(ensureFn);
        state.pending = {};
        state.pendingVersions.clear();
        state.uploadedVersions.clear();

        std::shared_lock atlasLock(atlasMutex);
        for (auto& [_, atlas] : atlas_map) {
            detail::enqueue_locked(state, atlas);
        }
    }

    inline void unregister_backend_uploader(core::ContextType type)
    {
        std::scoped_lock lock(detail::backendMutex);
        detail::backendStates.erase(type);
    }

    inline void enqueue_upload_for_all(const TextureAtlas& atlas)
    {
        std::scoped_lock lock(detail::backendMutex);
        for (auto& [_, state] : detail::backendStates) {
            if (!state.ensureFn)
                continue;
            detail::enqueue_locked(state, atlas);
        }
    }

    inline void process_pending_uploads(core::ContextType type)
    {
        if (detail::processingUploads)
            return;

        std::vector<detail::PendingUpload> tasks;
        std::function<void(const TextureAtlas&)> ensure;

        {
            std::scoped_lock lock(detail::backendMutex);
            auto it = detail::backendStates.find(type);
            if (it == detail::backendStates.end() || !it->second.ensureFn)
                return;

            ensure = it->second.ensureFn;
            auto& state = it->second;

            while (!state.pending.empty()) {
                const TextureAtlas* atlas = state.pending.front();
                state.pending.pop();
                if (!atlas)
                    continue;

                u64 version = atlas->version;
                if (auto pend = state.pendingVersions.find(atlas); pend != state.pendingVersions.end()) {
                    version = pend->second;
                    state.pendingVersions.erase(pend);
                }

                if (auto uploaded = state.uploadedVersions.find(atlas);
                    uploaded != state.uploadedVersions.end() && uploaded->second >= version) {
                    continue;
                }

                tasks.push_back({ atlas, version });
            }
        }

        if (tasks.empty())
            return;

        const bool previousProcessing = detail::processingUploads;
        auto previousBackend = detail::activeBackend;
        detail::processingUploads = true;
        detail::activeBackend = type;

        std::vector<detail::PendingUpload> completed;
        completed.reserve(tasks.size());

        for (auto& task : tasks) {
            try {
                ensure(*task.atlas);
                completed.push_back(task);
            }
            catch (const std::exception& e) {
                std::cerr << "[AtlasManager] Texture upload failed for '" << task.atlas->name
                    << "': " << e.what() << "\n";
            }
            catch (...) {
                std::cerr << "[AtlasManager] Texture upload failed for '" << task.atlas->name
                    << "' (unknown error)\n";
            }
        }

        detail::processingUploads = previousProcessing;
        detail::activeBackend = previousBackend;

        if (completed.empty())
            return;

        {
            std::scoped_lock lock(detail::backendMutex);
            auto it = detail::backendStates.find(type);
            if (it == detail::backendStates.end())
                return;

            for (const auto& task : completed) {
                it->second.uploadedVersions[task.atlas] = task.version;
            }
        }
    }

    inline void ensure_uploaded(const TextureAtlas& atlas)
    {
        enqueue_upload_for_all(atlas);

        if (detail::activeBackend && !detail::processingUploads) {
            process_pending_uploads(*detail::activeBackend);
        }
    }

} // namespace almondnamespace::atlasmanager
