#include "aopenglplatform.hpp"

#if defined(ALMOND_USING_OPENGL) && defined(_WIN32)

namespace almondnamespace::openglcontext::PlatformGL
{
    PlatformGLContext get_current() noexcept
    {
        PlatformGLContext ctx{};
        ctx.device = wglGetCurrentDC();
        ctx.context = wglGetCurrentContext();
        return ctx;
    }

    bool make_current(const PlatformGLContext& ctx) noexcept
    {
        if (!ctx.valid())
        {
            return wglMakeCurrent(nullptr, nullptr) == TRUE;
        }

        return wglMakeCurrent(ctx.device, ctx.context) == TRUE;
    }

    void clear_current() noexcept
    {
        wglMakeCurrent(nullptr, nullptr);
    }

    void swap_buffers(const PlatformGLContext& ctx) noexcept
    {
        if (ctx.device)
        {
            ::SwapBuffers(ctx.device);
        }
    }

    void* get_proc_address(const char* name) noexcept
    {
        if (!name)
        {
            return nullptr;
        }

        if (auto proc = reinterpret_cast<void*>(wglGetProcAddress(name)))
        {
            return proc;
        }

        static HMODULE openglModule = ::LoadLibraryA("opengl32.dll");
        return openglModule ? reinterpret_cast<void*>(::GetProcAddress(openglModule, name)) : nullptr;
    }
}

#endif // ALMOND_USING_OPENGL && _WIN32
