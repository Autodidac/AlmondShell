#pragma once

namespace almondnamespace::core {

    enum class ContextType {
        None = 0,
        OpenGL,
        SDL,
        SFML,
        RayLib,
        Vulkan,
        DirectX,
        Software,
        Custom,
        Noop
    };

} // namespace almondnamespace::core
