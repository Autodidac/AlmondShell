#include "ContextFactory.h"
#include "RenderContextOpenGL.h"
#include "WindowContextGLFW.h" // Include specific backend headers

namespace almond {
    WindowContext ContextFactory::CreateWindowContext(WindowBackend backend) {
        switch (backend) {
        case WindowBackend::Headless:
            throw std::runtime_error("Unsupported window backend.");//            return CreateHeadlessContext();
        case WindowBackend::GLFW:
            return CreateGLFWWindowContext();
        case WindowBackend::SFML:
            throw std::runtime_error("Unsupported window backend.");//            return CreateSFMLWindowContext();
        case WindowBackend::SDL:
            throw std::runtime_error("Unsupported window backend.");//            return CreateSDLWindowContext();
            // Add cases for other windowing backends here
        default:
            throw std::runtime_error("Unsupported window backend.");
        }
    }
    RenderContext ContextFactory::CreateRenderContext(RenderBackend backend) {
        switch (backend) {
        case RenderBackend::OpenGL:
            return CreateOpenGLRenderContext();
        case RenderBackend::Vulkan:
            return CreateOpenGLRenderContext();
        case RenderBackend::DirectX:
            return CreateOpenGLRenderContext();
        default:
            throw std::runtime_error("Unsupported render backend.");
        }
    }
    // External DLL functionality
    WindowContext* CreateWindowContext(WindowBackend backend){ return nullptr; }
    RenderContext* CreateRenderContext(RenderBackend backend){ return nullptr; }
} // namespace almond
