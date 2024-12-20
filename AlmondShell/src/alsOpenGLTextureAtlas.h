#pragma once

#include "alsOpenGLTexture.h"
#include "alsImageLoader.h"  // Assuming ImageLoader is defined elsewhere

#ifdef ALMOND_USING_OPENGLTEXTURE

namespace almond {

    class OpenGLTextureAtlas {
    public:
        OpenGLTextureAtlas(int width, int height)
            : atlasWidth(width), atlasHeight(height) {
            glGenTextures(1, &atlasID);
            glBindTexture(GL_TEXTURE_2D, atlasID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);  // Unbind after initialization
        }

        ~OpenGLTextureAtlas() {
            glDeleteTextures(1, &atlasID);
        }

        void AddTexture(const std::string& filepath, int xOffset, int yOffset) {
            atlasID++;
            //OpenGLTexture texture(filepath, Texture::Format::RGBA8);

            image = ImageLoader::LoadImage(filepath);

            glBindTexture(GL_TEXTURE_2D, atlasID);
            glTexSubImage2D(GL_TEXTURE_2D, 0, xOffset, yOffset, image.width, image.height, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels.data());
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        void Bind() const {
            glBindTexture(GL_TEXTURE_2D, atlasID);
        }

        void Unbind() const {
            glBindTexture(GL_TEXTURE_2D, 0);
        }

    private:
        unsigned int atlasID;
        int atlasWidth, atlasHeight;
        ImageLoader::ImageData image;
    };

} // namespace almond
#endif