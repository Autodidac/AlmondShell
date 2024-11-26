// BasicRenderer.cpp
#include "RenderingSystem.h"

namespace almond {

    //GUI Rendering
    // Sample function to draw a rectangle
    void RenderingSystem::DrawRect(float x, float y, float width, float height, unsigned int color) {
        // Use your engine's graphics API to draw a filled rectangle
        // For example, using DirectX or OpenGL
    }

    void RenderingSystem::DrawText(float x, float y, const std::string& text, unsigned int color) {
        // Draw text at position (x, y) with the provided color
    }

    void RenderingSystem::DrawImage(float x, float y, float width, float height, void* texture) {
        // Render an image (your engine's texture handle is passed here)
    }

    void RenderingSystem::BeginDraw() {
        // Setup the rendering context
    }

    void RenderingSystem::EndDraw() {
        // Finalize rendering for this frame
    }
}