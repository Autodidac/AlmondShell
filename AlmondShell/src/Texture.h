#pragma once

#include <string>

namespace almond {

    class Texture {
    public:
        // Supported texture formats
        enum class Format {
            RGBA8,   // 8 bits per channel RGBA
            RGB8,    // 8 bits per channel RGB
            GREY8    // 8 bits per channel grayscale
        };

        virtual ~Texture() = default;

        // Factory method to create a texture based on the active graphics driver
        static Texture* Create(const std::string& filepath, Format format);
        static Texture* Create(const std::string& filepath, Format format, bool generateMipmaps = true);

        // Virtual interface for texture operations
        virtual void Bind(unsigned int slot = 0) const = 0;
        virtual void Unbind() const = 0;

        // Getters for texture properties
        virtual int GetWidth() const = 0;
        virtual int GetHeight() const = 0;
        virtual unsigned int GetID() const = 0;

    protected:
        Texture() = default;
    };

} // namespace almond
