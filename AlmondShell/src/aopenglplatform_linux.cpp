#include "aopenglplatform.hpp"

#if defined(ALMOND_USING_OPENGL) && defined(__linux__)

#include <dlfcn.h>

namespace almondnamespace::openglcontext::PlatformGL
{
    PlatformGLContext get_current() noexcept
    {
        PlatformGLContext ctx{};
        ctx.display = glXGetCurrentDisplay();
        ctx.drawable = glXGetCurrentDrawable();
        ctx.context = glXGetCurrentContext();
        return ctx;
    }

    bool make_current(const PlatformGLContext& ctx) noexcept
    {
        if (!ctx.valid())
        {
            if (auto* display = glXGetCurrentDisplay())
            {
                return glXMakeCurrent(display, None, nullptr) == True;
            }
            return true;
        }

        return glXMakeCurrent(ctx.display, ctx.drawable, ctx.context) == True;
    }

    void clear_current() noexcept
    {
        if (auto* display = glXGetCurrentDisplay())
        {
            glXMakeCurrent(display, None, nullptr);
        }
    }

    void swap_buffers(const PlatformGLContext& ctx) noexcept
    {
        if (ctx.display && ctx.drawable)
        {
            glXSwapBuffers(ctx.display, ctx.drawable);
        }
    }

    void* get_proc_address(const char* name) noexcept
    {
        if (!name)
        {
            return nullptr;
        }

        if (auto proc = glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(name)))
        {
            return reinterpret_cast<void*>(proc);
        }

        static void* libGL = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
        return libGL ? dlsym(libGL, name) : nullptr;
    }
}

#endif // ALMOND_USING_OPENGL && __linux__
