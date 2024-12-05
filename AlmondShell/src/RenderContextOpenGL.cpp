#include "RenderContextOpenGL.h"
//#include "Texture_OpenGL.h"
#ifdef ALMOND_USING_OPENGL

#include <iostream>
#include <fstream>
#include <sstream>

namespace almond {

    // Function to compile shader
    unsigned int CompileShader(unsigned int type, const char* source) {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        // Check for compilation errors
        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            throw std::runtime_error("Shader Compilation Failed: " + std::string(infoLog));
        }

        return shader;
    }

    // Function to create the shader program
    unsigned int CreateShaderProgram(const char* vertexSource, const char* fragmentSource) {
        // Compile vertex and fragment shaders
        unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
        unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);

        // Link shaders into a program
        unsigned int shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        // Check for linking errors
        int success;
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
            throw std::runtime_error("Shader Program Linking Failed: " + std::string(infoLog));
        }

        // Cleanup shaders after linking
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return shaderProgram;
    }

/*
    // Function to compile shader
    unsigned int CompileShader(unsigned int type, const char* source) {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        // Check for compilation errors
        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            throw std::runtime_error("Shader Compilation Failed: " + std::string(infoLog));
        }

        return shader;
    }

    // Function to load shader from file
    std::string LoadShaderFromFile(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open shader file: " + filePath);
        }

        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    // Function to create the shader program
    unsigned int CreateShaderProgram(const std::string& vertexPath, const std::string& fragmentPath) {
        std::string vertexShaderSource = LoadShaderFromFile(vertexPath);
        std::string fragmentShaderSource = LoadShaderFromFile(fragmentPath);

        // Compile vertex and fragment shaders
        unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, vertexShaderSource.c_str());
        unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource.c_str());

        // Link shaders into a program
        unsigned int shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        // Check for linking errors
        int success;
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
            throw std::runtime_error("Shader Program Linking Failed: " + std::string(infoLog));
        }

        // Cleanup shaders after linking
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return shaderProgram;
    }
*/
    RenderContext CreateOpenGLRenderContext() {
        RenderContext context;

        // Initialize OpenGL context
        context.initialize = [](void* windowHandle) {
            auto* glfwWindow = static_cast<GLFWwindow*>(windowHandle);
            if (!glfwWindow) {
                throw std::runtime_error("Invalid GLFW window handle.");
            }
            glfwMakeContextCurrent(glfwWindow);
            if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
                throw std::runtime_error("Failed to initialize OpenGL.");
            }
            glEnable(GL_DEPTH_TEST); // Enable depth testing

            // fix me
            glViewport(0, 0, 800, 600);  // Set the viewport to match your window size
            std::cout << "OpenGL initialized successfully.\n";
            };

        // Set clear color
        context.clearColor = [](float r, float g, float b, float a) {
            glClearColor(0.0f, 0.0f, 1.0f, 1.0f); // Blue background
            };

        // Clear the framebuffer
        context.clear = []() {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            };

        // Swap buffers for the given window handle
        context.swapBuffers = [](void* windowHandle) {
            auto* glfwWindow = static_cast<GLFWwindow*>(windowHandle);
            if (!glfwWindow) {
                throw std::runtime_error("Invalid GLFW window handle in swapBuffers.");
            }
            glfwSwapBuffers(glfwWindow);
            };

        // Setup and render a triangle
        context.setupTriangle = []() -> unsigned int {
            // Triangle vertex data
            float vertices[] = {
                0.0f,  0.5f, 0.0f,  // Top vertex
               -0.5f, -0.5f, 0.0f,  // Bottom-left vertex
                0.5f, -0.5f, 0.0f   // Bottom-right vertex
            };

            // Create and bind a Vertex Array Object (VAO)
            unsigned int VAO, VBO;
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);

            glBindVertexArray(VAO);

            // Bind and set vertex buffer
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

            // Define vertex attribute layout
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);

            // Unbind VAO
            glBindVertexArray(0);

            return VAO;
            };

        // Render triangle
/*        context.renderTriangle = [](unsigned int VAO) {
            unsigned int shaderProgram = CreateShaderProgram("../shaders/vertex_shader.glsl", "../shaders/fragment_shader.glsl");
            glUseProgram(shaderProgram);

            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 3); // Render the triangle
            glBindVertexArray(0);
            };
*/
// Render triangle
        context.renderTriangle = [](unsigned int VAO) {
            // Shader program (use vertex and fragment shader for rendering)
            const char* vertexShaderSource = R"(
                #version 330 core
                layout(location = 0) in vec3 aPos;
                layout(location = 1) in vec2 aTexCoord;

                out vec2 TexCoord;

                void main() {
                    gl_Position = vec4(aPos, 1.0);
                    TexCoord = aTexCoord;
                }
            )";

            const char* fragmentShaderSource = R"(
                #version 330 core
                in vec2 TexCoord;
                out vec4 FragColor;

                uniform float uTime; // Time parameter passed from the application

                void main() {
                    // Create a smooth gradient animation
                    float red = 0.5 + 0.5 * sin(uTime + TexCoord.x * 10.0);
                    float green = 0.5 + 0.5 * sin(uTime + TexCoord.y * 10.0);
                    float blue = 0.5 + 0.5 * sin(uTime + (TexCoord.x + TexCoord.y) * 5.0);

                    // Set the final color with a smooth transition
                    FragColor = vec4(red, green, blue, 1.0);
                }
            )";

            unsigned int shaderProgram = CreateShaderProgram(vertexShaderSource, fragmentShaderSource);
            // Assuming you have a shader program object `shaderProgram`
            float time = static_cast<float>(glfwGetTime());
            glUseProgram(shaderProgram);
            glUniform1f(glGetUniformLocation(shaderProgram, "uTime"), time);

            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 3); // Render the triangle
            glBindVertexArray(0);
            };

        return context;
    }

} // namespace almond

#endif