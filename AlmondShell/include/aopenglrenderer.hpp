/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   This file is part of the Almond Project.                 *
 *   AlmondShell - Modular C++ Framework                      *
 *                                                            *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
 *                                                            *
 *   Provided "AS IS", without warranty of any kind.          *
 *   Use permitted for Non-Commercial Purposes ONLY,          *
 *   without prior commercial licensing agreement.            *
 *                                                            *
 *   Redistribution Allowed with This Notice and              *
 *   LICENSE file. No obligation to disclose modifications.   *
 *                                                            *
 *   See LICENSE file for full terms.                         *
 *                                                            *
 **************************************************************/
 // aopenglrenderer.hpp
#pragma once

#include "aengineconfig.hpp"
//#include "aopenglcontext.hpp"
#include "aatlastexture.hpp"
#include "aspritehandle.hpp"
#include "aopengltextures.hpp" // AtlasGPU + gpu_atlases + ensure_uploaded
#include "aopenglquad.hpp"
#include "aopenglstate.hpp"

#include <span>
#include <iostream>

#if defined (ALMOND_USING_OPENGL)
namespace almondnamespace::openglrenderer
{
	//using almondnamespace::openglcontext::state::s_openglstate;

	struct RendererContext
	{
		enum class RenderMode {
			SingleTexture,
			TextureAtlas
		};
		RenderMode mode = RenderMode::TextureAtlas;
	};

	inline void check_gl_error(const char* location) {
		GLenum err = glGetError();
		if (err != GL_NO_ERROR) {
			std::cerr << "[GL ERROR] " << location << " error: 0x" << std::hex << err << std::dec << "\n";
		}
	}

	inline bool is_handle_live(const SpriteHandle& handle) noexcept {
		return handle.is_valid();
	}

    inline void debug_gl_state(GLuint shader, GLuint expectedVAO, GLuint expectedTex,
        GLint uUVRegionLoc, GLint uTransformLoc) noexcept
    {
        GLint boundVAO = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &boundVAO);
        std::cerr << "[Debug] Bound VAO: " << boundVAO << ", Expected VAO: " << expectedVAO << "\n";

        GLint activeTexUnit = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexUnit);
        std::cerr << "[Debug] Active Texture Unit: 0x" << std::hex << activeTexUnit << std::dec << "\n";

        GLint boundTex = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTex);
        std::cerr << "[Debug] Bound Texture: " << boundTex << ", Expected Texture: " << expectedTex << "\n";

        if (uUVRegionLoc >= 0)
        {
            GLfloat uvRegion[4] = {};
            glGetUniformfv(shader, uUVRegionLoc, uvRegion);
            std::cerr << "[Debug] Uniform uUVRegion: ("
                << uvRegion[0] << ", " << uvRegion[1] << ", "
                << uvRegion[2] << ", " << uvRegion[3] << ")\n";
        }

        if (uTransformLoc >= 0)
        {
            GLfloat transform[4] = {};
            glGetUniformfv(shader, uTransformLoc, transform);
            std::cerr << "[Debug] Uniform uTransform: ("
                << transform[0] << ", " << transform[1] << ", "
                << transform[2] << ", " << transform[3] << ")\n";
        }
    }

    inline void draw_debug_outline() noexcept
    {
        glUseProgram(s_openglstate.shader);

        if (s_openglstate.uUVRegionLoc >= 0)
            glUniform4f(s_openglstate.uUVRegionLoc, 0, 0, 1, 1);

        if (s_openglstate.uTransformLoc >= 0)
            glUniform4f(s_openglstate.uTransformLoc, -1, -1, 2, 2);

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindVertexArray(s_openglstate.vao);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        glLineWidth(2.0f);

        glDrawArrays(GL_LINE_LOOP, 0, 4);

        glBindVertexArray(0);
        glLineWidth(1.0f);
    }

    inline void draw_sprite(SpriteHandle handle, std::span<const TextureAtlas* const> atlases,
        float x, float y, float width, float height) noexcept
    {
        if (!is_handle_live(handle)) {
            std::cerr << "[DrawSprite] Invalid sprite handle.\n";
            return;
        }

        if (width <= 0.f || height <= 0.f) {
            std::cerr << "[DrawSprite] Non-positive draw size requested.\n";
            return;
        }

        const int atlasIdx = static_cast<int>(handle.atlasIndex);
        const int localIdx = static_cast<int>(handle.localIndex);

        if (atlasIdx < 0 || atlasIdx >= static_cast<int>(atlases.size())) {
            std::cerr << "[DrawSprite] Atlas index out of bounds: " << atlasIdx << '\n';
            return;
        }

        const TextureAtlas* atlas = atlases[static_cast<size_t>(atlasIdx)];
        if (!atlas) {
            std::cerr << "[DrawSprite] Null atlas pointer at index: " << atlasIdx << '\n';
            return;
        }

        if (localIdx < 0 || localIdx >= static_cast<int>(atlas->entries.size())) {
            std::cerr << "[DrawSprite] Sprite index out of bounds: " << localIdx << '\n';
            return;
        }

        auto& backend = opengltextures::get_opengl_backend();
        auto& glState = backend.glState;

        if (glState.shader == 0 || glState.vao == 0) {
            std::cerr << "[DrawSprite] OpenGL state not initialized.\n";
            return;
        }

        opengltextures::ensure_uploaded(*atlas);
        auto gpuIt = backend.gpu_atlases.find(atlas);
        if (gpuIt == backend.gpu_atlases.end() || gpuIt->second.textureHandle == 0) {
            std::cerr << "[DrawSprite] GPU texture not available for atlas '" << atlas->name << "'\n";
            return;
        }

        const GLuint tex = gpuIt->second.textureHandle;

        GLint viewport[4] = { 0, 0, 1, 1 };
        glGetIntegerv(GL_VIEWPORT, viewport);
        const float viewportWidth = viewport[2] > 0 ? static_cast<float>(viewport[2]) : 1.f;
        const float viewportHeight = viewport[3] > 0 ? static_cast<float>(viewport[3]) : 1.f;

        const auto& entry = atlas->entries[static_cast<size_t>(localIdx)];
        const float u0 = entry.region.u1;
        const float v0 = entry.region.v1;
        const float du = entry.region.u2 - entry.region.u1;
        const float dv = entry.region.v2 - entry.region.v1;

        const float centerX = x + width * 0.5f;
        const float centerY = y + height * 0.5f;
        const float ndcX = (centerX / viewportWidth) * 2.f - 1.f;
        const float ndcY = 1.f - (centerY / viewportHeight) * 2.f;
        const float ndcW = (width / viewportWidth) * 2.f;
        const float ndcH = (height / viewportHeight) * 2.f;

        glUseProgram(glState.shader);
        glBindVertexArray(glState.vao);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);

        if (glState.uUVRegionLoc >= 0)
            glUniform4f(glState.uUVRegionLoc, u0, v0, du, dv);

        if (glState.uTransformLoc >= 0)
            glUniform4f(glState.uTransformLoc, ndcX, ndcY, ndcW, ndcH);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

        glDisable(GL_BLEND);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
    }
    inline void draw_quad(const openglcontext::Quad& quad, GLuint texture) {
        glUseProgram(s_openglstate.shader);

        // Set UV region for full quad (if needed)
        if (s_openglstate.uUVRegionLoc >= 0)
            glUniform4f(s_openglstate.uUVRegionLoc, 0.f, 0.f, 1.f, 1.f);

        // Bind texture explicitly
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        // Set sampler uniform once if not already done
        // glUniform1i(s_state.uTextureLoc, 0);

        glBindVertexArray(quad.vao());

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glDrawElements(GL_TRIANGLES, quad.index_count(), GL_UNSIGNED_INT, nullptr);

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glDisable(GL_BLEND);
    }


	inline void begin_frame() {
		// Set a clear color explicitly, e.g. blue
		glClearColor(0.f, 0.f, 1.f, 1.f);
		glViewport(0, 0, core::cli::window_width, core::cli::window_height);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	inline void end_frame() {
		//present();
	}

} // namespace almondnamespace::opengl
#endif // ALMOND_USING_OPENGL