#include "Texture.h"
#include "ImageLoader.h"

#include <glad/glad.h>
#include <stdexcept>

namespace almond {
    class OpenGLTexture : public Texture {
    public:
        OpenGLTexture(const std::string& filepath, Format format)
            : id(0), width(0), height(0), format(format) {
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

        void LoadTexture(const std::string& filepath) {
            auto image = ImageLoader::LoadImage(filepath);

            width = image.width;
            height = image.height;

            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            GLenum internalFormat = format == Format::RGBA8 ? GL_RGBA : GL_RGB;
            GLenum dataFormat = image.channels == 4 ? GL_RGBA : GL_RGB;

            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, image.pixels.data());
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    };

    Texture* Texture::Create(const std::string& filepath, Format format) {
        return new OpenGLTexture(filepath, format);
    }

} // namespace almond
