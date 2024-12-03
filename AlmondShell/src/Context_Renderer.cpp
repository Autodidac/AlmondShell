#include "Context_Renderer.h"

#include <glad/glad.h>
#include <stdexcept>
#include <iostream>

namespace almond {

    RenderContext::RenderContext(const std::string& title, float x, float y, float width, float height, unsigned int color, void* texture)
        : m_title(title), m_x(x), m_y(y), m_width(width), m_height(height), m_color(color), m_texture(texture), m_nativewindowhandle(nullptr), m_windowsystemcontext(nullptr) {
        Initialize(m_title, m_x, m_y, m_width, m_height, m_color, m_texture);
    }

    RenderContext::~RenderContext() {
        Cleanup();
    }

    void RenderContext::Initialize(const std::string& title, float x, float y, float width, float height, unsigned int color, void* texture) {
        // Initialize windowing and rendering API context (OpenGL example)
        // This assumes your engine has a custom window manager
        // Set up GLAD, framebuffer size, etc.
    // Create a window using the stored member variables
        m_nativewindowhandle = ContextWindow::CreateAlmondWindow(title, x, y, width, height);

        if (!m_nativewindowhandle) {
            throw std::runtime_error("Failed to create window.");
        }

        /*
        // Set up rendering API context
        m_windowsystemcontext = WindowSystem::CreateContext(m_nativewindowhandle);

        if (!m_windowsystemcontext) {
            throw std::runtime_error("Failed to create rendering context. inside cr");
        }*/

        // Set the initial color and bind texture if provided
        WindowSystem::SetClearColor(color);
        if (texture) {
            WindowSystem::BindTexture(static_cast<Texture*>(texture));
        }

        std::cout << "RenderContext initialized for: " << title << " at position ("
            << x << ", " << y << ") with dimensions " << width << "x" << height << "\n";
    }

    void RenderContext::Cleanup() {
        // Destroy the rendering context and associated resources
    }

    void RenderContext::BeginFrame() {

    }

    void RenderContext::EndFrame() {
        // Placeholder for buffer swap
    }

    void RenderContext::DrawRectangle(float x, float y, float width, float height, unsigned int color) {
        // Implement OpenGL rectangle drawing
    }

    void RenderContext::DrawAlmondText(const std::string& text, float x, float y, float width, float height, unsigned int color) {
        // Implement OpenGL text rendering (placeholder)
    }

    void RenderContext::DrawImage(float x, float y, float width, float height, void* texture) {
        // Implement OpenGL image rendering (placeholder)
    }

} // namespace almond
