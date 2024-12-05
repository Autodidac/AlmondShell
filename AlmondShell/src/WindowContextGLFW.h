#pragma once

#include "EngineConfig.h"
#include "ContextFactory.h"
#include "StringUtils.h"

#include <stdexcept>

#ifdef ALMOND_USING_GLFW

namespace almond {
    // Function declaration for creating a GLFW window context
    WindowContext CreateGLFWWindowContext();
} // namespace almond

#endif // ALMOND_USING_GLFW
