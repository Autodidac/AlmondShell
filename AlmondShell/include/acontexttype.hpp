#pragma once

namespace almondnamespace::core {

    enum class ContextType {
        None = 0,
        OpenGL,
        Software,
        SDL,
        SFML,
        RayLib,
        Custom,
        Noop
    };

} // namespace almondnamespace::core
