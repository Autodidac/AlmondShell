#include "Context_Window.h"

#include <GLFW/glfw3.h>

#include <stdexcept>
#include <iostream>

namespace almond {

    static bool glfwInitialized = false;

    ContextWindow::m_nativewindowhandle ContextWindow::CreateAlmondWindow(const std::string& title, float x, float y, float width, float height) {
        // Initialize GLFW if not already done
        if (!glfwInitialized) {
            if (!glfwInit()) {
                throw std::runtime_error("Failed to initialize GLFW.");
            }
            glfwInitialized = true;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        // Create GLFW window
        GLFWwindow* window = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title.c_str(), nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window.");
        }

        // Position the window
        glfwSetWindowPos(window, static_cast<int>(x), static_cast<int>(y));

        return static_cast<m_nativewindowhandle>(window);
    }

    void ContextWindow::DestroyWindow(m_nativewindowhandle window) {
        GLFWwindow* glfwWindow = static_cast<GLFWwindow*>(window);
        if (glfwWindow) {
            glfwDestroyWindow(glfwWindow);
        }

        // Terminate GLFW if all windows are closed
        glfwTerminate();
        glfwInitialized = false;
    }

    void ContextWindow::PollEvents() {
        glfwPollEvents();
    }

} // namespace almond
