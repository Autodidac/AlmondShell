
#include "GLFWContext.h"

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <stdexcept>
#include <vector>

namespace almond {
#ifdef ALMOND_USING_GLFW

    GLFWwindow* glfwWindow = nullptr;

    void initGLFW() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        glfwWindow = glfwCreateWindow(800, 600, "GLFW with OpenGL", nullptr, nullptr);
        if (!glfwWindow) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }

        glfwMakeContextCurrent(glfwWindow);
        glfwSwapInterval(1); // Enable vsync

        std::cout << "GLFW and OpenGL initialized successfully" << std::endl;
    }

    bool processGLFW() {
        if (glfwWindowShouldClose(glfwWindow)) {
            return false;
        }
        glfwPollEvents();
        return true;
    }

    void cleanupGLFW() {
        if (glfwWindow) {
            glfwDestroyWindow(glfwWindow);
        }
        glfwTerminate();
    }
#endif
}
