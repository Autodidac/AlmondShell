#pragma once

#include <string>
#include <type_traits>

namespace almond {

    template<typename T>
    concept RenderContextConcept = requires(T context, const std::string & text, float x, float y, float width, float height, unsigned int color, void* texture) {
        { context.BeginFrame() } -> std::same_as<void>;
        { context.EndFrame() } -> std::same_as<void>;
        { context.DrawRectangle(x, y, width, height, color) } -> std::same_as<void>;
        { context.DrawAlmondText(text, x, y, width, height, color) } -> std::same_as<void>;
        { context.DrawImage(x, y, width, height, texture) } -> std::same_as<void>;
    };
} // namespace almond
