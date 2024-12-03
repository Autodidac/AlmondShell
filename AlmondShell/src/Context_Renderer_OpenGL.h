#pragma once

//#include "Context_Renderer.h"
#include "Context_Concept.h"

#include <string>

namespace almond {
    class OpenGLRenderContext {
    public:
        OpenGLRenderContext(const std::string& title, float x, float y, float width, float height, unsigned int color, void* texture);
        ~OpenGLRenderContext();

        void BeginFrame();
        void EndFrame();
        void DrawRectangle(float x, float y, float width, float height, unsigned int color);
        void DrawAlmondText(const std::string& text, float x, float y, float width, float height, unsigned int color);
        void DrawImage(float x, float y, float width, float height, void* texture);
    };

    // The following line is not necessary unless you're explicitly specializing RenderContextConcept for this class.
    static_assert(RenderContextConcept<OpenGLRenderContext>, "OpenGLRenderContext does not satisfy RenderContextConcept");
} // namespace almond
