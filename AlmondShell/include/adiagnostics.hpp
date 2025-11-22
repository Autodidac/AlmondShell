
// AlmondShell diagnostics helpers
// --------------------------------
// Lightweight utilities for reporting the current engine build
// configuration.  The helpers are header-only so they can be used
// by the updater target without touching the CMake target graph.

#pragma once

#include "aengineconfig.hpp"

#include <array>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "acontexttype.hpp"

namespace almond::diagnostics
{
    using almondnamespace::core::ContextType;

    struct EngineConfigurationSnapshot
    {
        bool single_parent_topology;
        bool using_sdl;
        bool using_sfml;
        bool using_raylib;
        bool using_software_renderer;
        bool using_opengl;
        bool using_vulkan;
        bool using_directx;
    };

    [[nodiscard]] inline EngineConfigurationSnapshot capture_engine_configuration()
    {
        EngineConfigurationSnapshot snapshot{};

#if defined(ALMOND_SINGLE_PARENT) && (ALMOND_SINGLE_PARENT != 0)
        snapshot.single_parent_topology = true;
#else
        snapshot.single_parent_topology = false;
#endif

#if defined(ALMOND_USING_SDL)
        snapshot.using_sdl = true;
#else
        snapshot.using_sdl = false;
#endif

#if defined(ALMOND_USING_SFML)
        snapshot.using_sfml = true;
#else
        snapshot.using_sfml = false;
#endif

#if defined(ALMOND_USING_RAYLIB)
        snapshot.using_raylib = true;
#else
        snapshot.using_raylib = false;
#endif

#if defined(ALMOND_USING_SOFTWARE_RENDERER)
        snapshot.using_software_renderer = true;
#else
        snapshot.using_software_renderer = false;
#endif

#if defined(ALMOND_USING_OPENGL)
        snapshot.using_opengl = true;
#else
        snapshot.using_opengl = false;
#endif

#if defined(ALMOND_USING_VULKAN)
        snapshot.using_vulkan = true;
#else
        snapshot.using_vulkan = false;
#endif

#if defined(ALMOND_USING_DIRECTX)
        snapshot.using_directx = true;
#else
        snapshot.using_directx = false;
#endif

        return snapshot;
    }

    struct ContextDimensionSnapshot
    {
        const void* contextId{};
        ContextType type{ ContextType::None };
        std::string_view backendName{};
        int logicalWidth{};
        int logicalHeight{};
        int framebufferWidth{};
        int framebufferHeight{};
        int virtualWidth{};
        int virtualHeight{};
    };

    [[nodiscard]] inline std::string_view to_string(ContextType type) noexcept
    {
        switch (type)
        {
        case ContextType::OpenGL:   return "OpenGL";
        case ContextType::SDL:      return "SDL";
        case ContextType::SFML:     return "SFML";
        case ContextType::RayLib:   return "Raylib";
        case ContextType::Vulkan:   return "Vulkan";
        case ContextType::DirectX:  return "DirectX";
        case ContextType::Software: return "Software";
        case ContextType::Custom:   return "Custom";
        case ContextType::Noop:     return "Noop";
        case ContextType::None:
        default:
            return "None";
        }
    }

    inline void log_context_dimensions_if_changed(const ContextDimensionSnapshot& snapshot,
        std::ostream& output = std::cout)
    {
        struct CachedDimensions
        {
            bool initialised{ false };
            ContextType type{ ContextType::None };
            std::string backendName{};
            int logicalWidth{};
            int logicalHeight{};
            int framebufferWidth{};
            int framebufferHeight{};
            int virtualWidth{};
            int virtualHeight{};
        };

        static std::mutex s_mutex;
        static std::unordered_map<const void*, CachedDimensions> s_cache;

        if (!snapshot.contextId)
        {
            return;
        }

        std::scoped_lock lock(s_mutex);
        auto& record = s_cache[snapshot.contextId];

        const bool changed = !record.initialised
            || record.type != snapshot.type
            || record.backendName != snapshot.backendName
            || record.logicalWidth != snapshot.logicalWidth
            || record.logicalHeight != snapshot.logicalHeight
            || record.framebufferWidth != snapshot.framebufferWidth
            || record.framebufferHeight != snapshot.framebufferHeight
            || record.virtualWidth != snapshot.virtualWidth
            || record.virtualHeight != snapshot.virtualHeight;

        if (!changed)
        {
            return;
        }

        record.initialised = true;
        record.type = snapshot.type;
        record.backendName = std::string(snapshot.backendName);
        record.logicalWidth = snapshot.logicalWidth;
        record.logicalHeight = snapshot.logicalHeight;
        record.framebufferWidth = snapshot.framebufferWidth;
        record.framebufferHeight = snapshot.framebufferHeight;
        record.virtualWidth = snapshot.virtualWidth;
        record.virtualHeight = snapshot.virtualHeight;

        output << "[Context][Dims] backend=" << (record.backendName.empty() ? "(unnamed)" : record.backendName)
            << " type=" << to_string(record.type)
            << " logical=" << record.logicalWidth << 'x' << record.logicalHeight
            << " framebuffer=" << record.framebufferWidth << 'x' << record.framebufferHeight
            << " virtual=" << record.virtualWidth << 'x' << record.virtualHeight
            << '\n';
    }

    inline void print_engine_configuration_summary(std::ostream& output = std::cout)
    {
        const EngineConfigurationSnapshot snapshot = capture_engine_configuration();

        output << "[Engine] Active context topology: "
            << (snapshot.single_parent_topology ? "Single parent window" : "Multiple top-level windows")
            << '\n';

        const std::array<std::pair<std::string_view, bool>, 7> renderers{ {
            {"SDL context", snapshot.using_sdl},
            {"SFML context", snapshot.using_sfml},
            {"Raylib context", snapshot.using_raylib},
            {"Software renderer", snapshot.using_software_renderer},
            {"OpenGL renderer", snapshot.using_opengl},
            {"Vulkan renderer", snapshot.using_vulkan},
            {"DirectX renderer", snapshot.using_directx},
        } };

        output << "[Engine] Enabled integrations:";
        bool first = true;
        for (const auto& [label, enabled] : renderers)
        {
            if (!enabled)
            {
                continue;
            }

            output << (first ? ' ' : ', ') << label;
            first = false;
        }

        if (first)
        {
            output << " none";
        }

        output << '\n';

        if (!snapshot.using_opengl && !snapshot.using_software_renderer && !snapshot.using_raylib
            && !snapshot.using_sdl)
        {
            output << "[Engine][Warning] No primary renderer or context is enabled."
                << " Update aengineconfig.hpp before launching.\n";
        }
    }
} // namespace almond::diagnostics