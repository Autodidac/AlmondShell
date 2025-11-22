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
//#include "acontextmultiplexer.hpp" // BackendMap, windows
#include "awindowdata.hpp"

#include "aplatformpump.hpp"    // platform::pump_events()
#include "aatlasmanager.hpp"
#include "arobusttime.hpp"      // RobustTime
#include "ainput.hpp"

#include "aopengltextures.hpp" // ensure_uploaded, AtlasGPU, gpu_atlases
#include "aopenglplatform.hpp"

#include <algorithm>
#include <format>
#include <iostream>
#include <stdexcept>
#include <functional>
//#include <mutex>
//#include <queue>
#include <vector>
//#include <cstdint>
#include <utility>
#include <string>
//#include <cstdio>

#if defined(__linux__)
#    include <X11/Xlib.h>
#    include <X11/Xutil.h>
#    include <GL/glx.h>
#    include <GL/glxext.h>
#endif

namespace almondnamespace::openglcontext
{
    namespace detail
    {
#if defined(__linux__)
        inline ::Window to_xwindow(HWND handle) noexcept
        {
            return static_cast<::Window>(reinterpret_cast<uintptr_t>(handle));
        }

        inline HWND from_xwindow(::Window window) noexcept
        {
            return reinterpret_cast<HWND>(static_cast<uintptr_t>(window));
        }
#endif

        inline PlatformGL::PlatformGLContext state_to_platform_context(const almondnamespace::openglstate::OpenGL4State& state) noexcept
        {
            PlatformGL::PlatformGLContext ctx{};
#if defined(_WIN32)
            ctx.device = state.hdc;
            ctx.context = state.hglrc;
#elif defined(__linux__)
            ctx.display = state.display;
            ctx.drawable = state.drawable ? state.drawable : state.window;
            ctx.context = state.glxContext;
#endif
            return ctx;
        }

        inline PlatformGL::PlatformGLContext context_to_platform_context(const core::Context* ctx) noexcept
        {
            PlatformGL::PlatformGLContext result{};
            if (!ctx)
            {
                return result;
            }

#if defined(_WIN32)
            result.device = ctx->hdc;
            result.context = ctx->hglrc;
#elif defined(__linux__)
            result.display = static_cast<Display*>(ctx->hdc);
            result.drawable = static_cast<GLXDrawable>(reinterpret_cast<uintptr_t>(ctx->hwnd));
            result.context = static_cast<GLXContext>(ctx->hglrc);
#endif
            return result;
        }
    }

    // — Helpers
    inline void* LoadGLFunc(const char* name)
    {
        return PlatformGL::get_proc_address(name);
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

    inline void destroy_quad_pipeline(almondnamespace::openglstate::OpenGL4State& glState) noexcept
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

    inline const char* select_glsl_version(GLint major, GLint minor)
    {
        struct VersionCandidate
        {
            GLint major;
            GLint minor;
            const char* shaderVersion;
        };

        static constexpr VersionCandidate versions[] = {
            {4, 6, "#version 460 core"},
            {4, 5, "#version 450 core"},
            {4, 4, "#version 440 core"},
            {4, 3, "#version 430 core"},
            {4, 2, "#version 420 core"},
            {4, 1, "#version 410 core"},
            {4, 0, "#version 400 core"},
            {3, 3, "#version 330 core"},
        };

        for (const auto& candidate : versions)
        {
            if (major > candidate.major || (major == candidate.major && minor >= candidate.minor))
            {
                return candidate.shaderVersion;
            }
        }

        return "#version 330 core";
    }

    inline std::pair<GLint, GLint> parse_gl_version_string(const char* versionStr) noexcept
    {
        if (!versionStr)
            return { 0, 0 };

        std::string_view sv{ versionStr };

        // Trim leading whitespace
        const auto first_non_ws = sv.find_first_not_of(" \t\n\r");
        if (first_non_ws == std::string_view::npos)
            return { 0, 0 };
        sv.remove_prefix(first_non_ws);

        // Find first digit (skip "OpenGL ES ", "OpenGL ES 3.2", etc.)
        const auto first_digit = sv.find_first_of("0123456789");
        if (first_digit == std::string_view::npos)
            return { 0, 0 };
        sv.remove_prefix(first_digit);

        const auto dot_pos = sv.find('.');
        if (dot_pos == std::string_view::npos)
            return { 0, 0 };

        std::string_view major_part = sv.substr(0, dot_pos);
        sv.remove_prefix(dot_pos + 1);

        // Minor is the consecutive digits after the dot
        const auto minor_end = sv.find_first_not_of("0123456789");
        std::string_view minor_part = sv.substr(0, minor_end);

        auto parse_decimal = [](std::string_view part, GLint& out) noexcept -> bool {
            if (part.empty())
                return false;

            GLint value = 0;
            for (unsigned char ch : part)
            {
                if (ch < '0' || ch > '9')
                    return false;
                value = static_cast<GLint>(value * 10 + (ch - '0'));
            }
            out = value;
            return true;
            };

        GLint major = 0;
        GLint minor = 0;
        if (!parse_decimal(major_part, major) || !parse_decimal(minor_part, minor))
            return { 0, 0 };

        return { major, minor };
    }

    inline bool build_quad_pipeline(almondnamespace::openglstate::OpenGL4State& glState)
    {
        destroy_quad_pipeline(glState);

        try {
            GLint major = 0;
            GLint minor = 0;
            glGetIntegerv(GL_MAJOR_VERSION, &major);
            glGetIntegerv(GL_MINOR_VERSION, &minor);

            if (major == 0 && minor == 0)
            {
                const auto* versionStr = reinterpret_cast<const char*>(glGetString(GL_VERSION));
                auto [parsedMajor, parsedMinor] = parse_gl_version_string(versionStr);
                major = parsedMajor;
                minor = parsedMinor;
            }

            if (major == 0 && minor == 0)
            {
                major = 3;
                minor = 3;
            }

            const char* versionDirective = select_glsl_version(major, minor);

            std::cerr << "[OpenGL] Using GLSL directive '" << versionDirective
                << "' for GL context " << major << '.' << minor << "\n";

            std::string vs_source = std::string(versionDirective) + R"(

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

            std::string fs_source = std::string(versionDirective) + R"(
        in vec2 vUV;
        out vec4 outColor;

        uniform sampler2D uTexture;

        void main() {
            outColor = texture(uTexture, vUV);
        }
    )";

            glState.shader = linkProgram(vs_source.c_str(), fs_source.c_str());
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

    inline bool ensure_quad_pipeline(almondnamespace::openglstate::OpenGL4State& glState)
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
        if (initialized)
            return true;
        initialized = true;

        auto& backend = almondnamespace::opengltextures::get_opengl_backend();
        auto& glState = backend.glState;

        std::cerr << "[OpenGL Init] Starting initialization\n";

        ctx->width = static_cast<int>(w);
        ctx->height = static_cast<int>(h);
        ctx->virtualWidth = ctx->width;
        ctx->virtualHeight = ctx->height;
        ctx->framebufferWidth = ctx->width;
        ctx->framebufferHeight = ctx->height;
        glState.width = w;
        glState.height = h;
        ctx->onResize = std::move(onResize);

        PlatformGL::ScopedContext contextGuard;

#if defined(_WIN32)
        HWND resolvedHwnd = nullptr;
        HDC resolvedHdc = nullptr;
        HGLRC resolvedHglrc = nullptr;

        if (ctx && ctx->windowData)
        {
            resolvedHwnd = ctx->windowData->hwnd;
            resolvedHdc = ctx->windowData->hdc;
            resolvedHglrc = ctx->windowData->glContext;
            std::cerr << "[OpenGL Init] Using ctx->windowData handles\n";
        }

        if (!resolvedHdc)
            resolvedHdc = wglGetCurrentDC();
        if (!resolvedHglrc)
            resolvedHglrc = wglGetCurrentContext();

        HWND resolvedParent = parentWnd ? parentWnd : resolvedHwnd;

        if (resolvedHwnd)
        {
            glState.hwnd = resolvedHwnd;
            std::cerr << "[OpenGL Init] Using resolved hwnd=" << glState.hwnd << "\n";
        }
        else if (parentWnd)
        {
            glState.hwnd = parentWnd;
            std::cerr << "[OpenGL Init] Using parent hwnd=" << parentWnd << "\n";
        }
        else
        {
            std::cerr << "[OpenGL Init] WARNING: No hwnd resolved; keeping existing glState.hwnd="
                << glState.hwnd << "\n";
        }

        glState.hdc = resolvedHdc;
        glState.hglrc = resolvedHglrc;
        glState.parent = resolvedParent;

        if (!glState.hwnd)
        {
            glState.hwnd = CreateWindowExW(
                0, L"AlmondChild", L"working",
                WS_CHILD | WS_VISIBLE | WS_BORDER,
                CW_USEDEFAULT, CW_USEDEFAULT, w, h,
                glState.parent, nullptr, nullptr, nullptr);
        }

        if (!glState.hwnd)
            throw std::runtime_error("[OpenGLContext] Failed to create child window");

        ShowWindow(glState.hwnd, SW_SHOW);

        if (ctx && glState.hwnd)
        {
            RECT client{};
            if (GetClientRect(glState.hwnd, &client))
            {
                const int logicalW = static_cast<int>((std::max)(static_cast<LONG>(1), client.right - client.left));
                const int logicalH = static_cast<int>((std::max)(static_cast<LONG>(1), client.bottom - client.top));
                ctx->width = logicalW;
                ctx->height = logicalH;
                ctx->virtualWidth = logicalW;
                ctx->virtualHeight = logicalH;
                ctx->framebufferWidth = logicalW;
                ctx->framebufferHeight = logicalH;
                glState.width = static_cast<unsigned int>(logicalW);
                glState.height = static_cast<unsigned int>(logicalH);
            }
        }

        glState.hdc = GetDC(glState.hwnd);
        if (!glState.hdc)
            throw std::runtime_error("GetDC failed");

        PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1,
            PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
            PFD_TYPE_RGBA, 32, 0,0,0,0,0,0,0,0,0,24,0,0,0,0,0,0,0 };

        int pf = ChoosePixelFormat(glState.hdc, &pfd);
        if (!pf)
            throw std::runtime_error("ChoosePixelFormat failed");
        if (!SetPixelFormat(glState.hdc, pf, &pfd))
            throw std::runtime_error("SetPixelFormat failed");

        HGLRC tmpContext = wglCreateContext(glState.hdc);
        if (!tmpContext)
            throw std::runtime_error("wglCreateContext failed");

        PlatformGL::ScopedContext tempScope;
        PlatformGL::PlatformGLContext tmpCtx{};
        tmpCtx.device = glState.hdc;
        tmpCtx.context = tmpContext;
        if (!tempScope.set(tmpCtx))
            throw std::runtime_error("wglMakeCurrent failed");

        auto wglCreateContextAttribsARB =
            reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(LoadGLFunc("wglCreateContextAttribsARB"));
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

        tempScope.release();
        wglDeleteContext(tmpContext);

        PlatformGL::PlatformGLContext finalCtx{};
        finalCtx.device = glState.hdc;
        finalCtx.context = glState.hglrc;
        if (!contextGuard.set(finalCtx))
            throw std::runtime_error("Failed to make OpenGL context current");

        if (!gladLoadGL())
            throw std::runtime_error("Failed to initialize GLAD");

        ctx->hwnd = glState.hwnd;
        ctx->hdc = glState.hdc;
        ctx->hglrc = glState.hglrc;

        std::cerr << "[OpenGL Init] Final handles: hwnd=" << glState.hwnd
            << " hdc=" << glState.hdc
            << " hglrc=" << glState.hglrc << "\n";
#elif defined(__linux__)
        Display* display = static_cast<Display*>(ctx ? ctx->hdc : nullptr);
        ::Window window = ctx && ctx->hwnd ? detail::to_xwindow(ctx->hwnd) : 0;
        GLXContext glxContext = static_cast<GLXContext>(ctx ? ctx->hglrc : nullptr);

        if (!display)
        {
            display = XOpenDisplay(nullptr);
            if (!display)
                throw std::runtime_error("[OpenGLContext] Failed to open X display");
            glState.ownsDisplay = true;
        }
        else
        {
            glState.ownsDisplay = false;
        }

        const int screen = DefaultScreen(display);
        int fbCount = 0;
        static int visualAttribs[] = {
            GLX_X_RENDERABLE, True,
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
            GLX_DOUBLEBUFFER, True,
            GLX_RED_SIZE, 8,
            GLX_GREEN_SIZE, 8,
            GLX_BLUE_SIZE, 8,
            GLX_ALPHA_SIZE, 8,
            GLX_DEPTH_SIZE, 24,
            0
        };

        GLXFBConfig* configs = glXChooseFBConfig(display, screen, visualAttribs, &fbCount);
        if (!configs || fbCount == 0)
        {
            if (configs)
                XFree(configs);
            throw std::runtime_error("[OpenGLContext] glXChooseFBConfig failed");
        }

        glState.fbConfig = configs[0];
        XFree(configs);

        XVisualInfo* vi = glXGetVisualFromFBConfig(display, glState.fbConfig);
        if (!vi)
            throw std::runtime_error("[OpenGLContext] glXGetVisualFromFBConfig failed");

        if (!window)
        {
            if (!glState.colormap)
            {
                glState.colormap = XCreateColormap(display, RootWindow(display, vi->screen), vi->visual, AllocNone);
                if (!glState.colormap)
                {
                    XFree(vi);
                    throw std::runtime_error("[OpenGLContext] Failed to create X colormap");
                }
                glState.ownsColormap = true;
            }

            XSetWindowAttributes swa{};
            swa.colormap = glState.colormap;
            swa.event_mask = ExposureMask | StructureNotifyMask;

            window = XCreateWindow(
                display,
                RootWindow(display, vi->screen),
                0, 0,
                static_cast<int>(w),
                static_cast<int>(h),
                0,
                vi->depth,
                InputOutput,
                vi->visual,
                CWColormap | CWEventMask,
                &swa);

            if (!window)
            {
                XFree(vi);
                throw std::runtime_error("[OpenGLContext] Failed to create X11 window");
            }

            XStoreName(display, window, "Almond OpenGL");
            XMapWindow(display, window);
            XFlush(display);
            glState.ownsWindow = true;
        }
        else
        {
            glState.ownsWindow = false;
        }

        XWindowAttributes attrs{};
        if (XGetWindowAttributes(display, window, &attrs))
        {
            ctx->width = (std::max)(1, attrs.width);
            ctx->height = (std::max)(1, attrs.height);
            ctx->virtualWidth = ctx->width;
            ctx->virtualHeight = ctx->height;
            ctx->framebufferWidth = ctx->width;
            ctx->framebufferHeight = ctx->height;
            glState.width = static_cast<unsigned int>(ctx->width);
            glState.height = static_cast<unsigned int>(ctx->height);
        }

        XFree(vi);

        if (!glxContext)
        {
            auto createContextAttribs = reinterpret_cast<PFNGLXCREATECONTEXTATTRIBSARBPROC>(LoadGLFunc("glXCreateContextAttribsARB"));
            int contextAttribs[] = {
                GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
                GLX_CONTEXT_MINOR_VERSION_ARB, 3,
                GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
                0
            };

            if (createContextAttribs)
            {
                glxContext = createContextAttribs(display, glState.fbConfig, nullptr, True, contextAttribs);
            }

            if (!glxContext)
            {
                glxContext = glXCreateNewContext(display, glState.fbConfig, GLX_RGBA_TYPE, nullptr, True);
            }

            if (!glxContext)
            {
                throw std::runtime_error("[OpenGLContext] Failed to create GLX context");
            }
            glState.ownsContext = true;
        }
        else
        {
            glState.ownsContext = false;
        }

        glState.display = display;
        glState.window = window;
        glState.drawable = window;
        glState.glxContext = glxContext;

        PlatformGL::PlatformGLContext finalCtx{};
        finalCtx.display = display;
        finalCtx.drawable = glState.drawable;
        finalCtx.context = glxContext;

        if (!contextGuard.set(finalCtx))
            throw std::runtime_error("[OpenGLContext] Failed to make GLX context current");

        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(PlatformGL::get_proc_address)))
            throw std::runtime_error("Failed to initialize GLAD");

        ctx->hwnd = detail::from_xwindow(window);
        ctx->hdc = reinterpret_cast<HDC>(display);
        ctx->hglrc = reinterpret_cast<HGLRC>(glxContext);

        std::cerr << "[OpenGL Init] Final handles: display=" << display
            << " drawable=" << glState.drawable
            << " context=" << glxContext << "\n";
#else
        (void)ctx;
        (void)parentWnd;
        (void)w;
        (void)h;
        throw std::runtime_error("[OpenGLContext] Unsupported platform");
#endif

        if (!contextGuard.success())
            throw std::runtime_error("[OpenGLContext] Failed to activate OpenGL context");

        if (!build_quad_pipeline(glState))
            throw std::runtime_error("Failed to build OpenGL quad pipeline");

        contextGuard.release();

        atlasmanager::register_backend_uploader(core::ContextType::OpenGL,
            [](const TextureAtlas& atlas)
            {
                opengltextures::ensure_uploaded(atlas);
            });

        std::cerr << "[OpenGL Init] Context sync complete\n";
        return true;
    }

    inline void opengl_present() {
        auto& backend = opengltextures::get_opengl_backend();
        auto& glState = backend.glState;
        const auto ctx = detail::state_to_platform_context(glState);
        PlatformGL::swap_buffers(ctx);
    }

    //   inline void opengl_pump() { almondnamespace::platform::pump_events(); }
    inline int  opengl_get_width() {
        auto& backend = opengltextures::get_opengl_backend();
#if defined(_WIN32)
        RECT r{ 0, 0, 0, 0 };
        if (backend.glState.hwnd && GetClientRect(backend.glState.hwnd, &r)) {
            backend.glState.width = static_cast<unsigned int>((std::max)(static_cast<LONG>(1), r.right - r.left));
            return static_cast<int>(backend.glState.width);
        }
#else
        if (backend.glState.width > 0) {
            return static_cast<int>(backend.glState.width);
        }
#endif
        return (std::max)(1, core::cli::window_width);
    }
    inline int  opengl_get_height() {
        auto& backend = opengltextures::get_opengl_backend();
#if defined(_WIN32)
        RECT r{ 0, 0, 0, 0 };
        if (backend.glState.hwnd && GetClientRect(backend.glState.hwnd, &r)) {
            backend.glState.height = static_cast<unsigned int>((std::max)(static_cast<LONG>(1), r.bottom - r.top));
            return static_cast<int>(backend.glState.height);
        }
#else
        if (backend.glState.height > 0) {
            return static_cast<int>(backend.glState.height);
        }
#endif
        return (std::max)(1, core::cli::window_height);
    }
    inline void opengl_clear(std::shared_ptr<core::Context> ctx) {
        const int fbW = (std::max)(1, ctx ? ctx->framebufferWidth : 0);
        const int fbH = (std::max)(1, ctx ? ctx->framebufferHeight : 0);
        glViewport(0, 0, fbW, fbH);
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

#if defined(_WIN32)
        if (ctx && ctx->hwnd)
        {
            RECT client{};
            if (GetClientRect(ctx->hwnd, &client))
            {
                const int logicalW = static_cast<int>((std::max)(static_cast<LONG>(1), client.right - client.left));
                const int logicalH = static_cast<int>((std::max)(static_cast<LONG>(1), client.bottom - client.top));
                ctx->width = logicalW;
                ctx->height = logicalH;
                ctx->virtualWidth = logicalW;
                ctx->virtualHeight = logicalH;
                ctx->framebufferWidth = logicalW;
                ctx->framebufferHeight = logicalH;
                glState.width = static_cast<unsigned int>(logicalW);
                glState.height = static_cast<unsigned int>(logicalH);
            }
            else
            {
                glState.width = static_cast<unsigned int>((std::max)(1, ctx->framebufferWidth));
                glState.height = static_cast<unsigned int>((std::max)(1, ctx->framebufferHeight));
            }
        }
        else
        {
            glState.width = static_cast<unsigned int>((std::max)(1, ctx ? ctx->framebufferWidth : 0));
            glState.height = static_cast<unsigned int>((std::max)(1, ctx ? ctx->framebufferHeight : 0));
        }
#elif defined(__linux__)
        if (ctx && ctx->hwnd && glState.display)
        {
            XWindowAttributes attrs{};
            if (XGetWindowAttributes(glState.display, detail::to_xwindow(ctx->hwnd), &attrs))
            {
                ctx->width = (std::max)(1, attrs.width);
                ctx->height = (std::max)(1, attrs.height);
                ctx->virtualWidth = ctx->width;
                ctx->virtualHeight = ctx->height;
                ctx->framebufferWidth = ctx->width;
                ctx->framebufferHeight = ctx->height;
                glState.width = static_cast<unsigned int>(ctx->width);
                glState.height = static_cast<unsigned int>(ctx->height);
            }
            else
            {
                glState.width = static_cast<unsigned int>((std::max)(1, ctx->framebufferWidth));
                glState.height = static_cast<unsigned int>((std::max)(1, ctx->framebufferHeight));
            }
        }
        else
        {
            glState.width = static_cast<unsigned int>((std::max)(1, ctx ? ctx->framebufferWidth : 0));
            glState.height = static_cast<unsigned int>((std::max)(1, ctx ? ctx->framebufferHeight : 0));
        }
#else
        glState.width = static_cast<unsigned int>((std::max)(1, ctx ? ctx->framebufferWidth : 0));
        glState.height = static_cast<unsigned int>((std::max)(1, ctx ? ctx->framebufferHeight : 0));
#endif

        atlasmanager::process_pending_uploads(core::ContextType::OpenGL);

        PlatformGL::ScopedContext contextGuard;
        auto desiredCtx = detail::context_to_platform_context(ctx.get());
        bool hasContext = desiredCtx.valid() ? contextGuard.set(desiredCtx) : false;

        if (!hasContext)
        {
            auto fallback = detail::state_to_platform_context(glState);
            if (fallback.valid())
            {
                hasContext = contextGuard.set(fallback);
#if defined(_WIN32)
                ctx->hdc = glState.hdc;
                ctx->hglrc = glState.hglrc;
#elif defined(__linux__)
                ctx->hdc = reinterpret_cast<HDC>(glState.display);
                ctx->hglrc = reinterpret_cast<HGLRC>(glState.glxContext);
                ctx->hwnd = detail::from_xwindow(glState.drawable ? glState.drawable : glState.window);
#endif
            }
        }

        if (!hasContext)
        {
            std::cerr << "[OpenGL] Context not ready for rendering\n";
            return false;
        }

        if (!ensure_quad_pipeline(glState))
        {
            std::cerr << "[OpenGL] Failed to ensure quad pipeline\n";
            queue.drain();
            return true;
        }

        std::vector<const TextureAtlas*> atlasesToReload;
        for (auto& [atlas, gpu] : backend.gpu_atlases)
        {
            const bool handleMissing = gpu.textureHandle == 0;
            const bool handleValid = !handleMissing && glIsTexture(gpu.textureHandle) == GL_TRUE;
            if (handleMissing || !handleValid)
            {
                if (handleValid)
                {
                    glDeleteTextures(1, &gpu.textureHandle);
                }
                gpu.textureHandle = 0;
                gpu.version = 0;
                atlasesToReload.push_back(atlas);
            }
        }

        for (const TextureAtlas* atlas : atlasesToReload)
        {
            if (atlas)
                opengltextures::upload_atlas_to_gpu(*atlas);
        }

        opengl_clear(ctx);

        if (glState.vao == 0 || glState.vbo == 0 || glState.ebo == 0)
        {
            std::cerr << "[OpenGL] VAO/VBO/EBO not initialized!\n";
            queue.drain();
            PlatformGL::swap_buffers(contextGuard.target());
            return true;
        }

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
        PlatformGL::swap_buffers(contextGuard.target());
        return true;
    }


    inline void opengl_cleanup(std::shared_ptr<core::Context> ctx) {
        auto& backend = opengltextures::get_opengl_backend();
        auto& glState = backend.glState;

#if defined(_WIN32)
        PlatformGL::clear_current();
        if (glState.hglrc) {
            wglDeleteContext(glState.hglrc);
            glState.hglrc = nullptr;
        }
        if (glState.hdc && glState.hwnd) {
            ReleaseDC(glState.hwnd, glState.hdc);
        }
        glState.hdc = nullptr;
        glState.hwnd = nullptr;
        glState.parent = nullptr;
        ctx->hwnd = nullptr;
        ctx->hdc = nullptr;
        ctx->hglrc = nullptr;
#elif defined(__linux__)
        PlatformGL::clear_current();
        if (glState.ownsContext && glState.display && glState.glxContext) {
            glXDestroyContext(glState.display, glState.glxContext);
        }
        if (glState.ownsWindow && glState.display && glState.window) {
            XDestroyWindow(glState.display, glState.window);
        }
        if (glState.ownsColormap && glState.display && glState.colormap) {
            XFreeColormap(glState.display, glState.colormap);
            glState.colormap = 0;
        }
        if (glState.ownsDisplay && glState.display) {
            XCloseDisplay(glState.display);
        }
        glState.display = nullptr;
        glState.window = 0;
        glState.drawable = 0;
        glState.glxContext = nullptr;
        glState.fbConfig = nullptr;
        glState.ownsDisplay = false;
        glState.ownsWindow = false;
        glState.ownsContext = false;
        glState.ownsColormap = false;
        ctx->hwnd = nullptr;
        ctx->hdc = nullptr;
        ctx->hglrc = nullptr;
#else
        (void)ctx;
#endif
    }


} // namespace almond::opengl

#endif // ALMOND_USING_OPENGL
