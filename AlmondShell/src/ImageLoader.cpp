#include "ImageLoader.h"

#include <fstream>
#include <stdexcept>
#include <cstring>

namespace almond {

    ImageLoader::ImageData ImageLoader::LoadImage(const std::string& filepath) {
        if (filepath.ends_with(".bmp")) {
            return LoadBMP(filepath);
        }
        else if (filepath.ends_with(".png")) {
            return LoadPNG(filepath);
        }
        else {
            throw std::runtime_error("Unsupported file format: " + filepath);
        }
    }

    ImageLoader::ImageData ImageLoader::LoadBMP(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to open BMP file: " + filepath);
        }

        // BMP Header
        char header[54];
        file.read(header, 54);

        if (std::memcmp(header, "BM", 2) != 0) {
            throw std::runtime_error("Invalid BMP file: " + filepath);
        }

        // Extract image dimensions
        int width = *reinterpret_cast<int*>(&header[18]);
        int height = *reinterpret_cast<int*>(&header[22]);
        int bitsPerPixel = *reinterpret_cast<short*>(&header[28]);

        if (bitsPerPixel != 24 && bitsPerPixel != 32) {
            throw std::runtime_error("Unsupported BMP bit depth: " + filepath);
        }

        int channels = bitsPerPixel / 8;

        // Read pixel data
        int dataSize = width * height * channels;
        std::vector<uint8_t> pixels(dataSize);
        file.seekg(*reinterpret_cast<int*>(&header[10])); // Pixel data offset
        file.read(reinterpret_cast<char*>(pixels.data()), dataSize);

        // BMP stores pixels bottom-to-top; flip vertically
        std::vector<uint8_t> flipped(dataSize);
        for (int y = 0; y < height; ++y) {
            std::memcpy(&flipped[(height - y - 1) * width * channels],
                &pixels[y * width * channels],
                width * channels);
        }

        return { width, height, channels, flipped };
    }

    ImageLoader::ImageData ImageLoader::LoadPNG(const std::string& filepath) {
        // For simplicity, let's support PNG loading later
        // PNG decoding requires parsing the format (chunks, filtering, etc.)
        throw std::runtime_error("PNG loading not implemented yet.");
    }

} // namespace almond
