#pragma once

#include <cstdint>

namespace almond {

    class WindowSystem {
    public:
        WindowSystem(); // Default constructor
        ~WindowSystem();

        using ContextHandle = void*;

        // Create a rendering context
        static ContextHandle CreateContext(void* m_nativewindowhandle);

        // Destroy a rendering context
        static void DestroyContext(ContextHandle context);

        // Set the viewport
        static void SetViewport(int x, int y, int width, int height);

        // Set the clear color
        static void SetClearColor(uint32_t color);

        // Clear the screen
        static void Clear();

        // Bind a texture
        static void BindTexture(void* texture);

        // Swap buffers
        static void SwapBuffers(void* m_nativewindowhandle);
    };

} // namespace almond


