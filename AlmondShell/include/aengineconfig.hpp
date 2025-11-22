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
#pragma once

// Almond Entry Point No-Op
//#define ALMOND_MAIN_HEADLESS // engine defined if not using almond internal automatic entry point - NO-OP essentially 

#ifndef ALMOND_MAIN_HANDLED // user defined if not using internal automatic entry point, similar to how SDL handles it
#ifndef ALMOND_MAIN_HEADLESS // engine config defined if not using internal automatic entry point
	#ifdef _WIN32 // Automatically use WinMain on Windows
		#define ALMOND_USING_WINMAIN // if using WinMain instead of int main
	#endif
#endif // ALMOND_MAIN_HEADLESS
#endif 

//#define RUN_CODE_INSPECTOR
//#define DEBUG_INPUT
//#define DEBUG_INPUT_OS
// 
//#define DEBUG_MOUSE_MOVEMENT
//#define DEBUG_MOUSE_COORDS
//#define DEBUG_TEXTURE_RENDERING_VERBOSE
//#define DEBUG_TEXTURE_RENDERING_VERY_VERBOSE
//#define DEBUG_WINDOW_VERBOSE
// 
// 
// --------------------
// Engine Context Config
// --------------------
// Contexts - Engine Backend(s) - Rendering backend(s)
// 
// for multiple contexts and windows, only if more than one backend is used, otherwise the first context will be used
#define ALMOND_SINGLE_PARENT 1  // 0 = multiple top-level windows, 1 = single parent + children
// Only one of each context will be used, if you want multiple contexts, you must define them in the engine config,

#define ALMOND_USING_SDL 1
//#define ALMOND_USING_RAYLIB 1
#define ALMOND_USING_SOFTWARE_RENDERER 1
#define ALMOND_USING_OPENGL 1

#if defined(ALMOND_FORCE_DISABLE_SDL)
#undef ALMOND_USING_SDL
#endif
#if defined(ALMOND_FORCE_ENABLE_SDL)
#undef ALMOND_USING_SDL
#define ALMOND_USING_SDL 1
#endif

#if defined(ALMOND_FORCE_DISABLE_RAYLIB)
#undef ALMOND_USING_RAYLIB
#endif
#if defined(ALMOND_FORCE_ENABLE_RAYLIB)
#undef ALMOND_USING_RAYLIB
#define ALMOND_USING_RAYLIB 1
#endif

#if defined(ALMOND_FORCE_DISABLE_SOFTWARE_RENDERER)
#undef ALMOND_USING_SOFTWARE_RENDERER
#endif
#if defined(ALMOND_FORCE_ENABLE_SOFTWARE_RENDERER)
#undef ALMOND_USING_SOFTWARE_RENDERER
#define ALMOND_USING_SOFTWARE_RENDERER 1
#endif

#if defined(ALMOND_FORCE_DISABLE_OPENGL)
#undef ALMOND_USING_OPENGL
#endif
#if defined(ALMOND_FORCE_ENABLE_OPENGL)
#undef ALMOND_USING_OPENGL
#define ALMOND_USING_OPENGL 1
#endif

//#define ALMOND_USING_VULKAN  // You must also set the context in the example
//#define ALMOND_USING_DIRECTX  // Currently Not Supported In AlmondShell, See AlmondEngine...

// Windowing backend (Uncomment ONLY ONE) - we've since abstracted this away not sure it'd be useful 
//#define ALMOND_USING_GLFW
//#define ALMOND_USING_VOLK

//////////////////////////////////////// Do Not Edit/Configure Below This Line //////////////////////////////////////////////////
//
//
//
// --------------------
// // Include Libraries
// --------------------
#ifdef ALMOND_USING_SFML
#define SFML_STATIC
	#include <SFML/Graphics.hpp>
#endif // ALMOND_USING_SFML

#ifdef ALMOND_USING_WINMAIN
#include "aframework.hpp"
#endif

// Don't include windows.h before Raylib big trouble
#ifdef ALMOND_USING_RAYLIB
#include <glad/glad.h>	// for GLAD - OpenGL loader

#if defined(_WIN32)
#include <GL/wglext.h>  // for WGL - OpenGL extensions WGL Loader
#endif
	// before raylib.h
//	#define NOMINMAX
//#define WIN32_LEAN_AND_MEAN

	//#define RAYLIB_NO_WINDOWS_H // Tells raylib NOT to include Windows.h internally

	// To avoid conflicting windows.h symbols with raylib, some flags are defined
	// WARNING: Those flags avoid inclusion of some Win32 headers that could be required
	// by user at some point and won't be included...
	//-------------------------------------------------------------------------------------

	// If defined, the following flags inhibit definition of the indicated items.
	//#define NOGDICAPMASKS     // CC_*, LC_*, PC_*, CP_*, TC_*, RC_
	////#define NOVIRTUALKEYCODES // VK_*
	//#define NOWINMESSAGES     // WM_*, EM_*, LB_*, CB_*
	//#define NOWINSTYLES       // WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
	//#define NOSYSMETRICS      // SM_*
	//#define NOMENUS           // MF_*
	//#define NOICONS           // IDI_*
	//#define NOKEYSTATES       // MK_*
	//#define NOSYSCOMMANDS     // SC_*
	//#define NORASTEROPS       // Binary and Tertiary raster ops
	//#define NOSHOWWINDOW      // SW_*
	//#define OEMRESOURCE       // OEM Resource values
	//#define NOATOM            // Atom Manager routines
	//#define NOCLIPBOARD       // Clipboard routines
	//#define NOCOLOR           // Screen colors
	//#define NOCTLMGR          // Control and Dialog routines
	//#define NODRAWTEXT        // DrawText() and DT_*
	//#define NOGDI             // All GDI defines and routines
	//#define NOKERNEL          // All KERNEL defines and routines
	//#define NOUSER            // All USER defines and routines
	////#define NONLS             // All NLS defines and routines
	//#define NOMB              // MB_* and MessageBox()
	//#define NOMEMMGR          // GMEM_*, LMEM_*, GHND, LHND, associated routines
	//#define NOMETAFILE        // typedef METAFILEPICT
	//#define NOMINMAX          // Macros min(a,b) and max(a,b)
	//#define NOMSG             // typedef MSG and associated routines
	//#define NOOPENFILE        // OpenFile(), OemToAnsi, AnsiToOem, and OF_*
	//#define NOSCROLL          // SB_* and scrolling routines
	//#define NOSERVICE         // All Service Controller routines, SERVICE_ equates, etc.
	//#define NOSOUND           // Sound driver routines
	//#define NOTEXTMETRIC      // typedef TEXTMETRIC and associated routines
	//#define NOWH              // SetWindowsHook and WH_*
	//#define NOWINOFFSETS      // GWL_*, GCL_*, associated routines
	//#define NOCOMM            // COMM driver routines
	//#define NOKANJI           // Kanji support stuff.
	//#define NOHELP            // Help engine interface.
	//#define NOPROFILER        // Profiler interface.
	//#define NODEFERWINDOWPOS  // DeferWindowPos routines
	//#define NOMCX             // Modem Configuration Extensions
	// Type required before windows.h inclusion
//typedef struct tagMSG* LPMSG;

// Type required by some unused function...
//typedef struct tagBITMAPINFOHEADER {
//	DWORD biSize;
//	LONG  biWidth;
//	LONG  biHeight;
//	WORD  biPlanes;
//	WORD  biBitCount;
//	DWORD biCompression;
//	DWORD biSizeImage;
//	LONG  biXPelsPerMeter;
//	LONG  biYPelsPerMeter;
//	DWORD biClrUsed;
//	DWORD biClrImportant;
//} BITMAPINFOHEADER, * PBITMAPINFOHEADER;
//
//#include <objbase.h>
//#include <mmreg.h>
//#include <mmsystem.h>
//
//// Some required types defined for MSVC/TinyC compiler
//#if defined(_MSC_VER) || defined(__TINYC__)
//#include "propidl.h"
//#endif

	// Include raylib header
	// NOTE: This must be included after the above flags to avoid conflicts with Windows.h
	//       and other headers that may be included by raylib.

	// NOTE: If you want to use raylib with OpenGL 3.3+ core profile, you must define
	//       RAYLIB_NO_OPENGL_ES2 before including raylib.h
#define RAYLIB_NO_WINDOW // Use raylib without window management
#define RAYLIB_STATIC
//#define RAYLIB_NO_OPENGL_ES2 // Use OpenGL 3.3+ core profile
//	#define RAYLIB_NO_OPENGL_ES3 // Use OpenGL 3.3+ core profile
//	#define RAYLIB_NO_OPENGL_11 // Use OpenGL 3.3+ core profile
//	#define RAYLIB_NO_OPENGL_21 // Use OpenGL 3.3+ core profile
//	#define RAYLIB_NO_OPENGL_30 // Use OpenGL 3.3+ core profile
	// Include raylib header
//#define RLAPI static inline
#define CloseWindow Raylib_CloseWindow  // Before #include <raylib.h>
#define ShowCursor Raylib_ShowCursor  // Before #include <raylib.h>
#define LoadImageW Raylib_LoadImageW
#define DrawTextW Raylib_DrawTextW
#define DrawTextExW Raylib_DrawTextExW
#define Rectangle Raylib_Rectangle
#define PlaySoundW Raylib_PlaySoundW
#include <raylib.h>
#undef CloseWindow
#undef Raylib_CloseWindow
#undef ShowCursor
#undef Raylib_ShowCursor
#undef DrawTextW
#undef Raylib_DrawTextW
#undef DrawTextExW
#undef Raylib_DrawTextExW
#undef LoadImageW
#undef Raylib_LoadImageW
#undef Rectangle
#undef Raylib_Rectangle
#undef PlaySoundW
#undef Raylib_PlaySoundW
	// Optionally include raylib extensions
	//#include <rcore.hpp> // for raylib core functions
	
//#include <raymath.h> // for raylib math functions
	//#include <rtext.hpp> // for raylib text functions
	//#include <rtextures.hpp> // for raylib texture functions
	//#include <rshapes.hpp> // for raylib shapes functions
	//#include <rshaders.hpp> // for raylib shaders functions
	//#include <rmodels.hpp> // for raylib models functions
	//#include <rmesh.hpp> // for raylib mesh functions
	//#include <rutils.hpp> // for raylib utils functions

#endif

#ifdef ALMOND_USING_OPENGL
	#include <glad/glad.h>	// for GLAD - OpenGL loader
#if defined(_WIN32)
	#if defined(__has_include) && __has_include(<glad/glad_wgl.h>)
		#include <glad/glad_wgl.h>
	#elif defined(__has_include) && __has_include(<GL/wglext.h>)
		#include <GL/wglext.h>  // WGL extensions (legacy path)
	#else
		// Minimal WGL definitions needed for context creation when no header is available
		#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
			#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
		#endif
		#ifndef WGL_CONTEXT_MINOR_VERSION_ARB
			#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
		#endif
		#ifndef WGL_CONTEXT_PROFILE_MASK_ARB
			#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
		#endif
		#ifndef WGL_CONTEXT_CORE_PROFILE_BIT_ARB
			#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
		#endif
		#ifndef PFNWGLCREATECONTEXTATTRIBSARBPROC
			typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);
		#endif
	#endif
#endif

#if defined(__linux__)

#endif
#endif

#ifdef ALMOND_USING_SDL
	#include <glad/glad.h> // for GLAD - OpenGL loader... SDL requires it's own backend
	#include <SDL3/SDL.h>
	#include <SDL3/SDL_version.h> // for SDL version checking

	#ifndef ALMOND_MAIN_HEADLESS // engine defined if not using internal almond automatic entry point, raylib requires it's own auto-internal-entry-point sometimes we need to disable ours
		#define SDL_MAIN_HANDLED  // we pass this to SDL also and can allow it to handle it automatically through SDL as well
	#endif

	//#define SDL_MAIN_USE_CALLBACKS
	#include <SDL3/SDL_main.h> // Include SDL_main.h for SDL main entry point handling

	#include <SDL3/SDL_scancode.h> // Include SDL_scancode.h for SDL_SCANCODE_COUNT
	#include <SDL3_image/SDL_image.h>

	#include <SDL3/SDL_events.h>
	#include <SDL3/SDL_video.h>
	#include <SDL3/SDL_render.h>
	#include <SDL3/SDL_surface.h>
	#include <SDL3/SDL_opengl.h> // for OpenGL context
	//#include <SDL3/SDL_vulkan.h> // for Vulkan context
#endif

#ifdef ALMOND_USING_VOLK
	#define VK_NO_PROTOTYPES
	#include <volk.h>
	#define ALMOND_USING_GLM
#endif

#ifdef ALMOND_USING_VULKAN_WITH_GLFW // if using glfw and vulkan
	#define GLFW_INCLUDE_VULKAN // This is not an Almond Macro, This is a GLFW Macro Required to use GLFW with Vulkan SDK
	#define ALMOND_USING_VULKAN
	//#define ALMOND_USING_GLM// if using glm 
#endif

#ifdef ALMOND_USING_GLM
	#include <glm/glm.hpp>
#endif

#ifdef ALMOND_USING_VULKAN
#include <vulkan/vulkan.h>
	#ifdef _WIN32
		#include <vulkan/vulkan_win32.h> // for winmain
	#endif
#endif