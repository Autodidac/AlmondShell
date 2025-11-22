/*************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗   *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗  *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║  *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║  *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝  *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝   *
 *                                                           *
 *   This file is part of the Almond Project                 *
 *                                                           *
 *   AlmondShell - Modular C++ Framework                     *
 *                                                           *
 *   Copyright (c) 2025 Adam Rushford                        *
 *   All rights reserved.                                    *
 *                                                           *
 *   This software is provided "as is," without              *
 *   warranty of any kind, express or implied,               *
 *   including but not limited to the warranties             *
 *   of merchantability, fitness for a particular            *
 *   purpose, and noninfringement. In no event               *
 *   shall the authors be liable for any claim,              *
 *   damages, or other liability, whether in an              *
 *   action of contract, tort, or otherwise,                 *
 *   arising from, out of, or in connection with             *
 *   the software or the use or other dealings               *
 *   in the software.                                        *
 *                                                           *
 *************************************************************/

#pragma once

#include "aengineconfig.hpp"

#if defined(ALMOND_USING_VULKAN)
#ifdef ALMOND_USING_GLM

#include <vector>

namespace almondnamespace::vulkan {

    struct Vertex {
        glm::vec2 pos;       // Position
        glm::vec3 color;     // Color (for non-textured objects)
        glm::vec2 texCoord;  // Texture coordinate (for textured objects)
    };

    // Basic triangle vertex data (with color and texture coordinates)
    inline std::vector<Vertex> getTriangleVertices() {
        return {
            {{0.0f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.5f, 0.0f}},  // Top vertex (Red)
            {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},  // Right vertex (Green)
            {{-0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // Left vertex (Blue)
        };
    }

    // Indices for drawing the triangle (header-only)
    inline std::vector<uint32_t> getTriangleIndices() {
        return { 0, 1, 2 }; // Triangular face using the three vertices
    }
}
#endif
#endif