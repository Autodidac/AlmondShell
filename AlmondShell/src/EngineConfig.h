#pragma once

// --------------------
// Rendering Backends
// --------------------

// Ensure no conflicts with previously defined macros
#ifdef ALMOND_USING_OPENGL
#undef ALMOND_USING_OPENGL
#endif
#ifdef ALMOND_USING_VULKAN
#undef ALMOND_USING_VULKAN
#endif
#ifdef ALMOND_USING_DIRECTX
#undef ALMOND_USING_DIRECTX
#endif

// Choose the rendering backend
#define ALMOND_USING_OPENGL
#ifdef ALMOND_USING_OPENGL
#include <glad/glad.h>
#endif

#ifdef ALMOND_USING_VULKAN
// Include Vulkan headers here
#endif

#ifdef ALMOND_USING_DIRECTX
// Include DirectX headers here
#endif

// --------------------
// Windowing Backends
// --------------------

// Ensure no conflicts with previously defined macros
#ifdef ALMOND_USING_SFML
#undef ALMOND_USING_SFML
#endif
#ifdef ALMOND_USING_GLFW
#undef ALMOND_USING_GLFW
#endif

// Choose the windowing backend
#define ALMOND_USING_GLFW
#ifdef ALMOND_USING_GLFW
#include <GLFW/glfw3.h>
#endif

#ifdef ALMOND_USING_SFML
#include <SFML/Window.hpp>
#endif


