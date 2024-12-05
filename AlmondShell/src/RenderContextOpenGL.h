
#include "ContextFactory.h"
#include "EngineConfig.h"

#include <stdexcept>
#ifdef ALMOND_USING_OPENGL

namespace almond {
    RenderContext CreateOpenGLRenderContext();

} // namespace almond
#endif