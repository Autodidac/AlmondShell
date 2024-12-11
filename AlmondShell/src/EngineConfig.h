#pragma once

	// Ensure no conflicts with previously defined macros
#ifdef ALMOND_USING_GLFW
#undef ALMOND_USING_GLFW
#endif
#ifdef ALMOND_USING_SDL
#undef ALMOND_USING_SDL
#endif
#ifdef ALMOND_USING_SFML
#undef ALMOND_USING_SFML
#endif
#ifdef ALMOND_USING_OPENGL
#undef ALMOND_USING_OPENGL
#endif
#ifdef ALMOND_USING_VULKAN
#undef ALMOND_USING_VULKAN
#endif
#ifdef ALMOND_USING_DIRECTX
#undef ALMOND_USING_DIRECTX
#endif

// --------------------
// Engine Context Config
// --------------------
// Uncomment ONLY ONE of these binds to run the engine using that library, opengl defaults to glfw
#define ALMOND_USING_OPENGL
#define ALMOND_USING_SDL
//#define ALMOND_USING_SFML

// -------------

// Not Implimented for AlmondShell, see AlmondEngine instead
//#define ALMOND_USING_VULKAN
//#define ALMOND_USING_DIRECTX


// --------------------
// Rendering Only Backend APIs
// --------------------
// Choose the rendering backend
#ifdef ALMOND_USING_OPENGL
#define ALMOND_USING_GLFW
#include <glad/glad.h>
#endif
#ifdef ALMOND_USING_VULKAN
// Include Vulkan headers here
#endif
#ifdef ALMOND_USING_DIRECTX
// Include DirectX headers here
#endif

// --------------------
// Backend APIs
// --------------------
// Choose the windowing backend
#ifdef ALMOND_USING_GLFW
#include <GLFW/glfw3.h>
#endif

#ifdef ALMOND_USING_SDL
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#endif

#ifdef ALMOND_USING_SFML
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp> // Include for SFML
#endif
