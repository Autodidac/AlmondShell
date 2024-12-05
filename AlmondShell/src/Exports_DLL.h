#pragma once

#include <string>

#ifdef _WIN32
// ENTRYPOINT_STATICLIB should be defined in both Almond build and user application 
// to use static linking
#ifndef ALMONDSHELL_STATICLIB
#ifdef ALMONDSHELL_DLL_EXPORTS
#define ALMONDSHELL_API __declspec(dllexport)
#else
#define ALMONDSHELL_API __declspec(dllimport)
#endif
#else
#define ALMONDSHELL_API  // Empty for static library usage
#endif
#else

// GCC visibility for other platforms
#if (__GNUC__ >= 4) && !defined(ALMONDSHELL_STATICLIB) && defined(ALMONDSHELL_DLL_EXPORTS)
#define ALMONDSHELL_API __attribute__((visibility("default")))
#else
#define ALMONDSHELL_API  // Empty for static library usage
#endif
#endif

// Define calling conventions based on platform compatibility
#if defined(_STDCALL_SUPPORTED)
#define ALECALLCONV __stdcall
#else
#define ALECALLCONV __cdecl
#endif  // _STDCALL_SUPPORTED

