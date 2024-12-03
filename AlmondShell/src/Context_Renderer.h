#pragma once

#include "Context_Concept.h"
#include "Context_Window.h"
#include "Context_Renderer_OpenGL.h"
#include "WindowSystem.h"
#include "Texture.h"

#include <string>

namespace almond {

    class RenderContext {
    public:
        RenderContext( const std::string& title, float x, float y, float width, float height, unsigned int color, void* texture);
        ~RenderContext();

        // Frame operations
        void BeginFrame();
        void EndFrame();

        // Drawing functions
        void DrawRectangle(float x, float y, float width, float height, unsigned int color);
        void DrawAlmondText(const std::string& text, float x, float y, float width, float height, unsigned int color);
        void DrawImage(float x, float y, float width, float height, void* texture);

    private:

        std::string m_title;
        void* m_texture;
        unsigned int m_color;
        float m_width;
        float m_height;
        float m_x;
        float m_y;

        // Platform-specific handles
        void* m_nativewindowhandle;
        void* m_windowsystemcontext;

        void Initialize(const std::string& title, float x, float y, float width, float height, unsigned int color, void* texture);
        void Cleanup();
    };

} // namespace almond
