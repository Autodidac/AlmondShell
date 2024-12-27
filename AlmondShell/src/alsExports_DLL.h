#pragma once

#include <string>

// API Visibility Macros
#ifdef _WIN32
    // Windows DLL export/import logic
    #ifndef ALMONDSHELL_STATICLIB
        #ifdef ALMONDSHELL_DLL_EXPORTS
            #define ALMONDSHELL_API __declspec(dllexport)
        #else
            #define ALMONDSHELL_API __declspec(dllimport)
        #endif
    #else
        #define ALMONDSHELL_API  // Static library, no special attributes
    #endif
#else
    // GCC/Clang visibility attributes
    #if (__GNUC__ >= 4) && !defined(ALMONDSHELL_STATICLIB) && defined(ALMONDSHELL_DLL_EXPORTS)
        #define ALMONDSHELL_API __attribute__((visibility("default")))
    #else
        #define ALMONDSHELL_API  // Static library or unsupported compiler
    #endif
#endif

// Calling Convention Macros
#ifndef _STDCALL_SUPPORTED
    #define ALECALLCONV __cdecl
#else
    #define ALECALLCONV __stdcall
#endif
