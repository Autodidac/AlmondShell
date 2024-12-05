#include "WindowContextGLFW.h"

#include <memory>
#include <stdexcept>
#include <string>

#ifdef ALMOND_USING_GLFW

namespace almond {

    WindowContext CreateGLFWWindowContext() {
        // Initialize GLFW
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW.");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        // Structure to manage the GLFW window and ensure proper cleanup
        struct GLFWContext {
            GLFWwindow* window = nullptr;

            ~GLFWContext() {
                if (window) {
                    glfwDestroyWindow(window);
                }
                glfwTerminate();
            }
        };

        auto glfwContext = std::make_shared<GLFWContext>();

        return {
            .createWindow = [glfwContext](const std::wstring& title, int width, int height) -> WindowHandle {
                std::string utf8Title = ConvertToUtf8(title); // Convert wide string to UTF-8
                glfwContext->window = glfwCreateWindow(width, height, utf8Title.c_str(), nullptr, nullptr);
                if (!glfwContext->window) {
                    throw std::runtime_error("Failed to create GLFW window.");
                }
                // Position the window
                glfwSetWindowPos(glfwContext->window, static_cast<int>(width), static_cast<int>(height));

                glfwMakeContextCurrent(glfwContext->window);
                return static_cast<WindowHandle>(glfwContext->window);
            },
            .pollEvents = []() {
                glfwPollEvents();
            },
            .shouldClose = [glfwContext]() -> bool {
                return glfwContext->window && glfwWindowShouldClose(glfwContext->window);
            },
            .getNativeHandle = [glfwContext]() -> WindowHandle {
                if (!glfwContext->window) {
                    throw std::runtime_error("GLFW window has not been created.");
                }
                return static_cast<WindowHandle>(glfwContext->window);
            }
        };
    }
} // namespace almond

#endif // ALMOND_USING_GLFW
