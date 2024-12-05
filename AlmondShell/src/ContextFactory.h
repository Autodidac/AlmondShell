#pragma once

#include "Exports_DLL.h"
#include <memory>
#include <string>
#include <functional>

namespace almond {

    using WindowHandle = void*; // Abstract handle, platform-specific type can be used

    // Functional WindowContext for different window backends
    struct WindowContext {
        using CreateWindowFn = std::function<WindowHandle(const std::wstring&, int, int)>;
        using PollEventsFn = std::function<void()>;
        using ShouldCloseFn = std::function<bool()>;
        using GetNativeHandleFn = std::function<WindowHandle()>;

        CreateWindowFn createWindow;
        PollEventsFn pollEvents;
        ShouldCloseFn shouldClose;
        GetNativeHandleFn getNativeHandle;
    };
/*
    struct RenderContext {
        using InitializeFn = std::function<void(WindowHandle)>;
        using ClearColorFn = std::function<void(float, float, float, float)>;
        using ClearFn = std::function<void()>;
        using SwapBuffersFn = std::function<void(WindowHandle)>;
        using RenderFn = std::function<void()>;

        InitializeFn initialize;
        ClearColorFn clearColor;
        ClearFn clear;
        SwapBuffersFn swapBuffers;
        RenderFn render;  // This will be used to issue general render commands
    };
*/
    // Functional RenderContext for different rendering backends
    struct RenderContext {
        using InitializeFn = std::function<void(void*)>;
        using ClearColorFn = std::function<void(float, float, float, float)>;
        using ClearFn = std::function<void()>;
        using SwapBuffersFn = std::function<void(void*)>;

        InitializeFn initialize;
        ClearColorFn clearColor;
        ClearFn clear;
        SwapBuffersFn swapBuffers;

        // Setup for rendering a triangle
        using SetupTriangleFn = std::function<unsigned int()>;
        SetupTriangleFn setupTriangle;
        using RenderTriangleFn = std::function<void(unsigned int)>;
        RenderTriangleFn renderTriangle;
    };
    // Enumeration for window and render backends
    enum class WindowBackend { Headless, GLFW, SFML, SDL };
    enum class RenderBackend { OpenGL, Vulkan, DirectX };

    // Factory to create window and render contexts
    class ContextFactory {
    public:
        static WindowContext CreateWindowContext(WindowBackend backend);
        static RenderContext CreateRenderContext(RenderBackend backend);
    };

    // External declarations for use in both static and DLL contexts
    extern "C" ALMONDSHELL_API WindowContext* CreateWindowContext(WindowBackend backend);
    extern "C" ALMONDSHELL_API RenderContext* CreateRenderContext(RenderBackend backend);

} // namespace almond
