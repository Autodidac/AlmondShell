#pragma once

#include "alsEngineConfig.h"
#include "alsOpenGLTexture.h"
#include "alsOpenGLShader.h"
#include "alsOpenGLTextureAtlas.h"
#include "alsMesh.h"

#include <vector>
#include <unordered_map>
#ifdef ALMOND_USING_GLFW

namespace almond {
    class Renderer; // Forward declaration

    enum class RenderMode {
        SingleTexture,
        TextureAtlas
    };

    class RenderCommand {
    public:
        enum class CommandType {
            Draw,
            SetUniform
        };

        RenderCommand(CommandType type) : type(type) {}
        virtual ~RenderCommand() = default;

        CommandType GetType() const { return type; }
        virtual void Execute(ShaderProgram* shader) const = 0;

    private:
        CommandType type;
    };

    // Use a base class for uniforms for extensibility
    class UniformCommand : public RenderCommand {
    public:
        UniformCommand(const std::string& name) : RenderCommand(CommandType::SetUniform), uniformName(name) {}
        virtual ~UniformCommand() = default;

        std::string GetName() const {
            return uniformName;
        }

    protected:
        std::string uniformName;
    };

    // A concrete class for setting a vec2 uniform
    class SetVec2Uniform : public UniformCommand {
    public:
        SetVec2Uniform(const std::string& name, const glm::vec2& value)
            : UniformCommand(name), value(value) {
        }

        void Execute(ShaderProgram* shader) const override {
            shader->SetUniform(uniformName, value);
        }

    private:
        glm::vec2 value;
    };


    class DrawCommand : public RenderCommand {
    public:

        DrawCommand(std::shared_ptr<Mesh> mesh, int textureSlot, RenderMode renderMode, float x, float y)
            : RenderCommand(CommandType::Draw), mesh(mesh), textureSlot(textureSlot), renderMode(renderMode), x(x), y(y)
        {
        }

        void Execute(ShaderProgram* shader) const override {
            if (renderMode == RenderMode::SingleTexture) {
                shader->SetUniform("position", glm::vec2(x, y));
                glDrawElements(GL_TRIANGLES, mesh->GetIndexCount(), GL_UNSIGNED_INT, nullptr);
            }
            else if (renderMode == RenderMode::TextureAtlas)
            {
                shader->SetUniform("position", glm::vec2(x, y));
                glDrawArrays(GL_TRIANGLE_STRIP, 0, mesh->GetVertexCount());
            }
        }

        const std::shared_ptr<Mesh> GetMesh() const { return mesh; }
        int GetTextureSlot() const { return textureSlot; }

    private:
        std::shared_ptr<Mesh> mesh;
        int textureSlot;
        float x, y;
        RenderMode renderMode;
    };

    class Renderer {
    public:

        Renderer() {
            shader = std::make_shared<ShaderProgram>("../../shaders/vert.glsl", "../../shaders/frag.glsl");
        }

        Renderer(OpenGLTextureAtlas& atlas) : textureAtlas(&atlas), renderMode(RenderMode::TextureAtlas) {
            shader = std::make_shared<ShaderProgram>("../../shaders/vert.glsl", "../../shaders/frag.glsl");
        }

        ~Renderer() {}

        void SetRenderMode(RenderMode mode) {
            renderMode = mode;
        }

        void SetTextureAtlas(OpenGLTextureAtlas& atlas)
        {
            textureAtlas = &atlas;
            renderMode = RenderMode::TextureAtlas;
        }


        void AddTexture(const std::string& filepath, int xOffset, int yOffset) {
            if (renderMode == RenderMode::TextureAtlas) {
                // Check if texturePath is not empty and load a texture
                if (!filepath.empty()) {
                    textureAtlas->AddTexture(filepath, xOffset, yOffset);
                }
                else {
                    // Handle the case where no path is provided
                    // Optionally load a default texture or log a warning
                    std::cerr << "Texture path is empty. Using default texture." << std::endl;
                    //textures.push_back("../../../images/bmp3.bmp", almond::Texture::Format::RGBA8);  // Add a default texture (needs to be defined in OpenGLTexture)
                }
            }
            else
            {
                if (!filepath.empty()) {
                    OpenGLTexture texture(filepath, almond::Texture::Format::RGBA8, true);  // Load from file
                    textures.push_back(texture);  // Store the loaded texture
                }
                else {
                    // Handle the case where no path is provided
                    // Optionally load a default texture or log a warning
                    std::cerr << "Texture path is empty. Using default texture." << std::endl;
                    //textures.push_back("../../../images/bmp3.bmp", almond::Texture::Format::RGBA8);  // Add a default texture (needs to be defined in OpenGLTexture)
                }
            }
        }

        void AddMesh(const std::string& name, const std::vector<float>& vertices, const std::vector<unsigned int>& indices)
        {
            if (meshes.find(name) == meshes.end()) {
                meshes.emplace(name, std::make_shared<Mesh>(vertices, indices));
            }
            else
            {
                std::cerr << "Mesh with the name: " << name << " already exists!" << std::endl;
            }
        }

        std::shared_ptr<Mesh> GetMesh(const std::string& name)
        {
            if (meshes.find(name) == meshes.end()) {
                return nullptr;
            }
            return meshes[name];
        }

        OpenGLTexture& GetTexture(const std::string& texturePath) {
            for (auto& tex : textures) {
                if (tex.GetPath() == texturePath) {
                    return tex;
                }
            }
            throw std::runtime_error("Texture not found in the renderer.");
        }

        OpenGLTexture& GetTextureData(const std::string& texturePath) {
            for (auto& tex : textures) {
                if (tex.GetPath() == texturePath) {
                    return tex;
                }
            }
            throw std::runtime_error("Texture not found in the renderer.");
        }

        void DrawBatch(const std::vector<std::shared_ptr<almond::RenderCommand>>& commands) {
            if (commands.empty()) return;

            shader->Use();
            shader->SetUniform("textureSampler", 0);
            shader->SetUniform("scale", scaleValue);

            if (renderMode == RenderMode::TextureAtlas) {
                if (textureAtlas)
                    textureAtlas->Bind();
            }
            else {
                if (!textures.empty()) {
                    textures[0].Bind(0);
                }
                else
                {
                    std::cerr << "No single texture loaded to render!" << std::endl;
                    return;
                }
            }

            // Apply uniform commands
            for (const auto& cmd : commands)
            {
                if (cmd->GetType() == RenderCommand::CommandType::SetUniform)
                {
                    cmd->Execute(shader.get());
                }
            }

            // Apply draw commands
            for (const auto& cmd : commands) {
                if (auto drawCmd = dynamic_cast<const DrawCommand*>(cmd.get())) {
                    glBindVertexArray(SetupVAO(drawCmd->GetMesh()));
                    drawCmd->Execute(shader.get());
                    glBindVertexArray(0); // unbind the VAO
                }
            }

            if (renderMode == RenderMode::TextureAtlas) {
                if (textureAtlas)
                    textureAtlas->Unbind();
            }
            else {
                if (!textures.empty()) {
                    textures[0].Unbind();
                }
            }

        }

        void DrawSingleTexture() {
            if (renderMode == RenderMode::SingleTexture) {
                shader->Use();  // Use the shader program
                shader->SetUniform("textureSampler", 0);  // Set a texture uniform
                shader->SetUniform("scale", scaleValue);  // Set a scale uniform

                if (!textures.empty())
                {
                    textures[0].Bind(0); // Bind the single texture to slot 0
                    glBindVertexArray(VAO);
                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
                    glBindVertexArray(0);
                    textures[0].Unbind();  // Unbind texture
                }
                else
                {
                    std::cerr << "No single texture loaded to render!" << std::endl;
                    return;
                }
            }
        }

    private:
        unsigned int VAO = 0;
        OpenGLTexture* texture; // single texture pointer
        OpenGLTextureAtlas* textureAtlas = nullptr;  // Reference to the texture atlas
        std::vector<OpenGLTexture> textures;  // Store loaded textures
        mutable std::shared_ptr<ShaderProgram> shader;
        float scaleValue = 1.0f; // Can be adjusted as needed
        RenderMode renderMode = RenderMode::SingleTexture; // Current render mode (single texture or texture atlas)
        std::unordered_map<std::string, std::shared_ptr<Mesh>> meshes; // named mesh map


        unsigned int SetupVAO(const std::shared_ptr<Mesh>& mesh)
        {
            unsigned int VAO;
            glGenVertexArrays(1, &VAO);
            glBindVertexArray(VAO);

            glBindBuffer(GL_ARRAY_BUFFER, mesh->GetVBO());
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->GetEBO());

            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);

            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
            glEnableVertexAttribArray(1);
            glBindVertexArray(0);

            return VAO;
        }

};
} // namespace almond
#endif