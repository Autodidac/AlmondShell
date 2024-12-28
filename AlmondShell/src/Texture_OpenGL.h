#include "Texture.h"
#include "ImageLoader.h"  // Assuming this is your custom image loader
#include "EngineConfig.h"

#ifdef ALMOND_USING_OPENGLTEXTURE

#include <stdexcept>

namespace almond {

    class OpenGLTexture : public Texture {
    public:
        OpenGLTexture(const std::string& filepath, Format format, bool generateMipmaps = true)
            : id(0), width(0), height(0), format(format), generateMipmaps(generateMipmaps) {
            LoadTexture(filepath);
        }

        ~OpenGLTexture() override {
            glDeleteTextures(1, &id);
        }

        void Bind(unsigned int slot = 0) const override {
            glActiveTexture(GL_TEXTURE0 + slot);
            glBindTexture(GL_TEXTURE_2D, id);
        }

        void Unbind() const override {
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        int GetWidth() const override { return width; }
        int GetHeight() const override { return height; }
        unsigned int GetID() const override { return id; }

    private:
        unsigned int id;
        int width;
        int height;
        Format format;
        bool generateMipmaps;

        void LoadTexture(const std::string& filepath) {
            // Load the image using your custom image loader
            auto image = ImageLoader::LoadImage(filepath);

            // Check if image is loaded correctly
            if (image.pixels.empty()) {
                throw std::runtime_error("Failed to load image: " + filepath);
            }

            width = image.width;
            height = image.height;

            // Generate OpenGL texture and configure it
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);

            // Set texture parameters
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            // Set texture wrap mode (can be changed to GL_CLAMP_TO_EDGE by default for most use cases)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Set internal and data formats based on image channels
            GLenum internalFormat = (format == Format::RGBA8) ? GL_RGBA : GL_RGB;
            GLenum dataFormat = (image.channels == 4) ? GL_RGBA : GL_RGB;

            // Upload texture data to GPU
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, image.pixels.data());

            // Optionally generate mipmaps (only if required)
            if (generateMipmaps) {
                glGenerateMipmap(GL_TEXTURE_2D);
            }

            // Check for OpenGL errors
            GLenum error = glGetError();
            if (error != GL_NO_ERROR) {
                throw std::runtime_error("OpenGL error during texture creation: " + std::to_string(error));
            }
        }
    };

    Texture* Texture::Create(const std::string& filepath, Format format, bool generateMipmaps) {
        return new OpenGLTexture(filepath, format, generateMipmaps);
    }

} // namespace almond
#endif
