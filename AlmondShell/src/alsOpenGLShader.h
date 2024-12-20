#pragma once

#include "alsEngineConfig.h"

//#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#ifdef ALMOND_USING_GLFW


namespace almond {

    class ShaderProgram {
    public:
        ShaderProgram(const std::string& vertexPath, const std::string& fragmentPath) {
            std::string vertexCode;
            std::string fragmentCode;

            // Read vertex and fragment shader source code
            try {
                std::ifstream vShaderFile(vertexPath);
                std::ifstream fShaderFile(fragmentPath);

                std::stringstream vShaderStream, fShaderStream;
                vShaderStream << vShaderFile.rdbuf();
                fShaderStream << fShaderFile.rdbuf();

                vertexCode = vShaderStream.str();
                fragmentCode = fShaderStream.str();
            }
            catch (std::ifstream::failure& e) {
                std::cerr << "ERROR: Shader file not successfully read: " << e.what() << std::endl;
            }

            const char* vShaderCode = vertexCode.c_str();
            const char* fShaderCode = fragmentCode.c_str();

            // Compile shaders
            unsigned int vertex, fragment;

            vertex = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vertex, 1, &vShaderCode, nullptr);
            glCompileShader(vertex);
            CheckCompileErrors(vertex, "VERTEX");

            fragment = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fragment, 1, &fShaderCode, nullptr);
            glCompileShader(fragment);
            CheckCompileErrors(fragment, "FRAGMENT");

            // Link shaders into program
            ID = glCreateProgram();
            glAttachShader(ID, vertex);
            glAttachShader(ID, fragment);
            glLinkProgram(ID);
            CheckCompileErrors(ID, "PROGRAM");

            // Delete shaders after linking
            glDeleteShader(vertex);
            glDeleteShader(fragment);
        }

        void Use() const {
            glUseProgram(ID);
        }

        GLuint GetID() const {
            return ID;
        }

        void SetUniform(const std::string& name, int value) const {
            glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
        }

        void SetUniform(const std::string& name, float value) const {
            glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
        }

        void SetUniform(const std::string& name, const glm::vec2& value) {
            glUniform2f(glGetUniformLocation(ID, name.c_str()), value.x, value.y);
        }

        void SetUniform(const std::string& name, const glm::vec3& value) {
            glUniform3f(glGetUniformLocation(ID, name.c_str()), value.x, value.y, value.z);
        }


    private:
        GLuint ID;

        void CheckCompileErrors(GLuint shader, const std::string& type) const {
            int success;
            char infoLog[1024];
            if (type != "PROGRAM") {
                glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
                if (!success) {
                    glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
                    std::cerr << "ERROR: Shader Compilation Error (" << type << "):\n" << infoLog << std::endl;
                }
            }
            else {
                glGetProgramiv(shader, GL_LINK_STATUS, &success);
                if (!success) {
                    glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
                    std::cerr << "ERROR: Shader Program Linking Error:\n" << infoLog << std::endl;
                }
            }
        }
    };

} // namespace almond
#endif