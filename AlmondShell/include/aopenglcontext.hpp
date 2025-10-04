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
// aopenglcontext.hpp
#pragma once

#include "aplatform.hpp"
#include "aengineconfig.hpp"    // brings in <windows.h>, <glad/glad.h>, etc.

#if defined(ALMOND_USING_OPENGL)

//#include "acontext.hpp"       // Context, ContextType
#include "acontextmultiplexer.hpp" // BackendMap, windows
#include "awindowdata.hpp"

//#include "acontextwindow.hpp"   // WindowContext::getWindowHandle()
#include "aplatformpump.hpp"    // platform::pump_events()
#include "aatlasmanager.hpp"
#include "arobusttime.hpp"      // RobustTime
#include "ainput.hpp"

//#include "aopenglstate.hpp"  // brings in the actual inline s_state
//#include "aopenglcontextinput.hpp"   // WndProcHook()
//#include "aopenglrenderer.hpp" // RendererContext, RenderMode
#include "aopengltextures.hpp" // ensure_uploaded, AtlasGPU, gpu_atlases 

//#include <array>
//#include <chrono>
//#include <format>
//#include <iostream>
//#include <stdexcept>
//#include <bitset>
//#include <mutex>
//#include <queue>
//#include <functional>
#include <format>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <mutex>
#include <queue>
#include <vector>

namespace almondnamespace::openglcontext
{
    // — Helpers
    inline void* LoadGLFunc(const char* name)
    {
        if (auto p = (void*)wglGetProcAddress(name)) return p;
        static HMODULE m = LoadLibraryA("opengl32.dll");
        return m ? (void*)GetProcAddress(m, name) : nullptr;
    }

    inline GLuint compileShader(GLenum type, const char* src)
    {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            char log[512]{};
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            throw std::runtime_error(std::format("Shader compile error: {}\nSource:\n{}", log, src));
        }
        return shader;
    }

    inline GLuint linkProgram(const char* vsSrc, const char* fsSrc)
    {
        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vsSrc);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fsSrc);
        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);
        GLint linked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            char log[512]{};
            glGetProgramInfoLog(program, sizeof(log), nullptr, log);
            throw std::runtime_error(std::format("Program link error: {}", log));
        }
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return program;
    }

    inline void destroy_quad_pipeline(OpenGL4State& glState) noexcept
    {
        if (glState.shader && glIsProgram(glState.shader)) {
            glDeleteProgram(glState.shader);
        }
        glState.shader = 0;
        glState.uUVRegionLoc = -1;
        glState.uTransformLoc = -1;
        glState.uSamplerLoc = -1;

        if (glState.vao && glIsVertexArray(glState.vao)) {
            glDeleteVertexArrays(1, &glState.vao);
        }
        if (glState.vbo && glIsBuffer(glState.vbo)) {
            glDeleteBuffers(1, &glState.vbo);
        }
        if (glState.ebo && glIsBuffer(glState.ebo)) {
            glDeleteBuffers(1, &glState.ebo);
        }

        glState.vao = 0;
        glState.vbo = 0;
        glState.ebo = 0;
    }

    inline bool build_quad_pipeline(OpenGL4State& glState)
    {
        destroy_quad_pipeline(glState);

        try {
            constexpr auto vs_source = R"(
        #version 460 core

        layout(location = 0) in vec2 aPos;       // [-0.5..0.5] quad coords
        layout(location = 1) in vec2 aTexCoord;  // [0..1] UV coords

        uniform vec4 uTransform; // xy = center in NDC, zw = size in NDC
        uniform vec4 uUVRegion;  // xy = UV offset, zw = UV size

        out vec2 vUV;

        void main() {
            vec2 pos = aPos * uTransform.zw + uTransform.xy;
            gl_Position = vec4(pos, 0.0, 1.0);
            vUV = uUVRegion.xy + aTexCoord * uUVRegion.zw;
        }
    )";

            constexpr auto fs_source = R"(
        #version 460 core
        in vec2 vUV;
        out vec4 outColor;

        uniform sampler2D uTexture;

        void main() {
            outColor = texture(uTexture, vUV);
        }
    )";

            glState.shader = linkProgram(vs_source, fs_source);
        }
        catch (const std::exception& ex) {
            std::cerr << "[OpenGL] Failed to build quad pipeline: " << ex.what() << "\n";
            destroy_quad_pipeline(glState);
            return false;
        }

        glState.uUVRegionLoc = glGetUniformLocation(glState.shader, "uUVRegion");
        glState.uTransformLoc = glGetUniformLocation(glState.shader, "uTransform");
        glState.uSamplerLoc = glGetUniformLocation(glState.shader, "uTexture");

        if (glState.uSamplerLoc >= 0) {
            glUseProgram(glState.shader);
            glUniform1i(glState.uSamplerLoc, 0);
            glUseProgram(0);
        }

        glGenVertexArrays(1, &glState.vao);
        glGenBuffers(1, &glState.vbo);
        glGenBuffers(1, &glState.ebo);

        if (!glState.vao || !glState.vbo || !glState.ebo) {
            std::cerr << "[OpenGL] Failed to allocate quad geometry buffers\n";
            destroy_quad_pipeline(glState);
            return false;
        }

        glBindVertexArray(glState.vao);
        glBindBuffer(GL_ARRAY_BUFFER, glState.vbo);

        constexpr float quadVerts[] = {
            -0.5f, -0.5f,    0.0f, 0.0f,
             0.5f, -0.5f,    1.0f, 0.0f,
             0.5f,  0.5f,    1.0f, 1.0f,
            -0.5f,  0.5f,    0.0f, 1.0f
        };

        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glState.ebo);

        constexpr unsigned int quadIndices[] = {
            0, 1, 2,
            2, 3, 0
        };
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        std::cerr << "[OpenGL] Quad pipeline (shader/VAO/VBO/EBO) rebuilt\n";
        return true;
    }

    inline bool ensure_quad_pipeline(OpenGL4State& glState)
    {
        const bool shaderValid = glState.shader != 0 && glIsProgram(glState.shader) == GL_TRUE;
        const bool vaoValid = glState.vao != 0 && glIsVertexArray(glState.vao) == GL_TRUE;
        const bool vboValid = glState.vbo != 0 && glIsBuffer(glState.vbo) == GL_TRUE;
        const bool eboValid = glState.ebo != 0 && glIsBuffer(glState.ebo) == GL_TRUE;

        if (shaderValid && vaoValid && vboValid && eboValid)
            return true;

        return build_quad_pipeline(glState);
    }

    // — Engine hooks
    inline bool opengl_initialize(std::shared_ptr<core::Context> ctx,
        HWND parentWnd = nullptr,
        unsigned int w = 800,
        unsigned int h = 600,
        std::function<void(int, int)> onResize = nullptr)
    {
        static bool initialized = false;
        if (initialized) return true;
        initialized = true;


        auto& backend = almondnamespace::opengltextures::get_opengl_backend();
        auto& glState = backend.glState;

        std::cerr << "[OpenGL Init] Starting initialization\n";

        // -----------------------------------------------------------------
        // Step 1: Resolve window/context handles
        // -----------------------------------------------------------------
        HWND   resolvedHwnd = nullptr;
        HDC    resolvedHdc = nullptr;
        HGLRC  resolvedHglrc = nullptr;

        if (ctx && ctx->windowData) {
            resolvedHwnd = ctx->windowData->hwnd;
            resolvedHdc = ctx->windowData->hdc;
            resolvedHglrc = ctx->windowData->glContext;
            std::cerr << "[OpenGL Init] Using ctx->windowData handles\n";
        }

        // Fallbacks: pull directly from current thread
        if (!resolvedHdc)   resolvedHdc = wglGetCurrentDC();
        if (!resolvedHglrc) resolvedHglrc = wglGetCurrentContext();

        // Parent HWND priority
        HWND resolvedParent = parentWnd ? parentWnd : resolvedHwnd;

        // Assign hwnd only if we actually resolved one
        if (resolvedHwnd) {
            glState.hwnd = resolvedHwnd;
            std::cerr << "[OpenGL Init] Using resolved hwnd=" << resolvedHwnd << "\n";
        }
        else if (parentWnd) {
            glState.hwnd = parentWnd;
            std::cerr << "[OpenGL Init] Using parent hwnd=" << parentWnd << "\n";
        }
        else {
            // Leave glState.hwnd unchanged if it was already set earlier
            std::cerr << "[OpenGL Init] WARNING: No hwnd resolved; keeping existing glState.hwnd="
                << glState.hwnd << "\n";
        }

        // Always update HDC / HGLRC from current thread
        glState.hdc = resolvedHdc;
        glState.hglrc = resolvedHglrc;
        glState.parent = resolvedParent;

        std::cerr << "[OpenGL Init] Final handles: hwnd=" << glState.hwnd
            << " hdc=" << glState.hdc
            << " hglrc=" << glState.hglrc << "\n";

        // -----------------------------------------------------------------
        // Step 2: Cache size and callback into Context
        // -----------------------------------------------------------------
        ctx->width = static_cast<int>(w);
        ctx->height = static_cast<int>(h);
        ctx->onResize = std::move(onResize);


        if (!glState.hwnd) {
            glState.hwnd = CreateWindowEx(
                0, L"AlmondChild", L"working",
                WS_CHILD | WS_VISIBLE | WS_BORDER,
                CW_USEDEFAULT, CW_USEDEFAULT, w, h,
                glState.parent, nullptr, nullptr, nullptr);
        }

        if (!glState.hwnd)
            throw std::runtime_error("[OpenGLContext] Failed to create child window");

        ShowWindow(glState.hwnd, SW_SHOW);

        // Device context
        glState.hdc = GetDC(glState.hwnd);
        if (!glState.hdc)
            throw std::runtime_error("GetDC failed");

        PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1,
            PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
            PFD_TYPE_RGBA, 32, 0,0,0,0,0,0,0,0,0,24,0,0,0,0,0,0,0 };

        int pf = ChoosePixelFormat(glState.hdc, &pfd);
        if (!pf) throw std::runtime_error("ChoosePixelFormat failed");
        if (!SetPixelFormat(glState.hdc, pf, &pfd))
            throw std::runtime_error("SetPixelFormat failed");

        // Temporary context to load extensions
        HGLRC tmpContext = wglCreateContext(glState.hdc);
        if (!tmpContext) throw std::runtime_error("wglCreateContext failed");
        if (!wglMakeCurrent(glState.hdc, tmpContext))
            throw std::runtime_error("wglMakeCurrent failed");

        auto wglCreateContextAttribsARB =
            (PFNWGLCREATECONTEXTATTRIBSARBPROC)LoadGLFunc("wglCreateContextAttribsARB");
        if (!wglCreateContextAttribsARB)
            throw std::runtime_error("Failed to load wglCreateContextAttribsARB");

        int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
            WGL_CONTEXT_MINOR_VERSION_ARB, 6,
            WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };

        glState.hglrc = wglCreateContextAttribsARB(glState.hdc, nullptr, attribs);
        if (!glState.hglrc)
            throw std::runtime_error("Failed to create OpenGL 4.6 context");

        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(tmpContext);

        if (!wglMakeCurrent(glState.hdc, glState.hglrc))
            throw std::runtime_error("Failed to make OpenGL context current");

        if (!gladLoadGL())
            throw std::runtime_error("Failed to initialize GLAD");

        if (!build_quad_pipeline(glState))
            throw std::runtime_error("Failed to build OpenGL quad pipeline");

        // -----------------------------------------------------------------
        // Step 9: Hook texture uploads
        // -----------------------------------------------------------------
        atlasmanager::register_backend_uploader(core::ContextType::OpenGL,
            [](const TextureAtlas& atlas) {
                opengltextures::ensure_uploaded(atlas);
            });

        // -----------------------------------------------------------------
        // Step 10: Sync context with GL state
        // -----------------------------------------------------------------
        ctx->hwnd = glState.hwnd;
        ctx->hdc = glState.hdc;
        ctx->hglrc = glState.hglrc;

        std::cerr << "[OpenGL Init] Context sync complete\n";
        return true;
}

    inline void opengl_present() {
        auto& backend = opengltextures::get_opengl_backend();
        auto& glState = backend.glState;        // IMPORTANT: attach our static state
        SwapBuffers(glState.hdc);
    }

    //   inline void opengl_pump() { almondnamespace::platform::pump_events(); }
    inline int  opengl_get_width() { 
        RECT r; GetClientRect(s_openglstate.hwnd, &r); return r.right - r.left; 
    }
    inline int  opengl_get_height() { 
        RECT r; GetClientRect(s_openglstate.hwnd, &r); return r.bottom - r.top;
    }
    inline void opengl_clear(std::shared_ptr<core::Context> ctx) {
        glViewport(0, 0, ctx->width, ctx->height);
        glClearColor(0.235f, 0.235f, 0.235f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // ------------------------------------------------------
    // Per-frame process: clear, run commands, swap
    // ------------------------------------------------------
    inline bool opengl_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue)
    {
        auto& backend = opengltextures::get_opengl_backend();
        auto& glState = backend.glState;

        atlasmanager::process_pending_uploads(core::ContextType::OpenGL);

        if (!ctx->hdc || !ctx->hglrc) {
            std::cerr << "[OpenGL] Context not ready (hdc=" << ctx->hdc
                << ", hglrc=" << ctx->hglrc << ")\n";
            return false;
        }

        if (!wglMakeCurrent(ctx->hdc, ctx->hglrc)) {
            DWORD err = GetLastError();
            std::cerr << "[OpenGL] wglMakeCurrent failed (error " << err
                << ") attempting recovery\n";

            if (backend.glState.hdc && backend.glState.hglrc) {
                ctx->hdc = backend.glState.hdc;
                ctx->hglrc = backend.glState.hglrc;
            }

            if (!ctx->hdc || !ctx->hglrc || !wglMakeCurrent(ctx->hdc, ctx->hglrc)) {
                std::cerr << "[OpenGL] Unable to recover current context, skipping frame\n";
                return true;
            }
        }

        if (!ensure_quad_pipeline(glState)) {
            std::cerr << "[OpenGL] Failed to ensure quad pipeline\n";
            queue.drain();
            wglMakeCurrent(nullptr, nullptr);
            return true;
        }

        std::vector<const TextureAtlas*> atlasesToReload;
        for (auto& [atlas, gpu] : backend.gpu_atlases) {
            const bool handleMissing = gpu.textureHandle == 0;
            const bool handleValid = !handleMissing && glIsTexture(gpu.textureHandle) == GL_TRUE;
            if (handleMissing || !handleValid) {
                if (handleValid) {
                    glDeleteTextures(1, &gpu.textureHandle);
                }
                gpu.textureHandle = 0;
                gpu.version = 0;
                atlasesToReload.push_back(atlas);
            }
        }

        for (const TextureAtlas* atlas : atlasesToReload) {
            if (atlas)
                opengltextures::upload_atlas_to_gpu(*atlas);
        }

        // ─── Time & events ──────────────────────────────────────────────────────
        //static auto lastReal = std::chrono::steady_clock::now();
        //auto nowReal = std::chrono::steady_clock::now();
        //s_clockSystem.advanceTime(nowReal - lastReal);
        //lastReal = nowReal;

        //platform::pump_events();

        //// ─── FPS logging ────────────────────────────────────────────────────────
        //++s_frameCount;
        //if (s_pollTimer.elapsed() >= 0.3) {
        //    s_pollTimer.restart();
        //}
        //if (s_fpsTimer.elapsed() >= 1.0) {
        //    std::cout << "FPS: " << s_frameCount << "\n";
        //    s_frameCount = 0;
        //    s_fpsTimer.restart();
        //}


        opengl_clear(ctx);

        // --- Debug sanity: check VAO/EBO existence before use ---
        if (glState.vao == 0 || glState.vbo == 0 || glState.ebo == 0) {
            std::cerr << "[OpenGL] VAO/VBO/EBO not initialized!\n";
            queue.drain();
            SwapBuffers(ctx->hdc);
            wglMakeCurrent(nullptr, nullptr);
            return true;
        }

        // --- Update input each frame ---
      //  almondnamespace::input::poll_input();

        // Pipe global input into context functions
        ctx->is_key_held = [](almondnamespace::input::Key key) -> bool {
            return almondnamespace::input::is_key_held(key);
            };
        ctx->is_key_down = [](almondnamespace::input::Key key) -> bool {
            return almondnamespace::input::is_key_down(key);
            };
        ctx->is_mouse_button_held = [](almondnamespace::input::MouseButton btn) -> bool {
            return almondnamespace::input::is_mouse_button_held(btn);
            };
        ctx->is_mouse_button_down = [](almondnamespace::input::MouseButton btn) -> bool {
            return almondnamespace::input::is_mouse_button_down(btn);
            };

        queue.drain();
        SwapBuffers(ctx->hdc);

        wglMakeCurrent(nullptr, nullptr);
        return true;
    }


    inline void opengl_cleanup(std::shared_ptr<core::Context> ctx) {
        if (ctx->hglrc) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(ctx->hglrc);
            ctx->hglrc = nullptr;
        }
        if (ctx->hdc && ctx->hwnd) {
            ReleaseDC(ctx->hwnd, ctx->hdc);
            ctx->hdc = nullptr;
        }
    }


} // namespace almond::opengl

#endif // ALMOND_USING_OPENGL
