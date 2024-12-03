#pragma once

#include <string>
#include <memory>

namespace almond {

    class ContextWindow {
    public:
        // Platform-specific window handle (opaque pointer)
        using m_nativewindowhandle = void*;

        // Create a new window
        static m_nativewindowhandle CreateAlmondWindow(const std::string& title, float x, float y, float width, float height);

        // Destroy an existing window
        static void DestroyWindow(m_nativewindowhandle window);

        // Process events (for input and window management)
        static void PollEvents();
    };

} // namespace almond
