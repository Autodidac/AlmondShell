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
// aplatformpump.hpp
#pragma once

#include "aengineconfig.hpp"   // All ENGINE-specific includes
#include "ainput.hpp"

#if defined(_WIN32)
    #include "aframework.hpp"
#elif defined(__APPLE__) && defined(__MACH__)
    #import <Cocoa/Cocoa.h>   // compile as ObjC++
#elif defined(__linux__)
    #include <X11/Xlib.h>      // ensure you link -lX11
#if defined(__linux__)
namespace almondnamespace::core
{
    void HandleX11Configure(::Window window, int width, int height);
}
#endif
#elif defined(__ANDROID__)
    #include <android_native_app_glue.h>
#elif defined(__EMSCRIPTEN__)
    #include <emscripten.h>
#else
    #error "Unsupported platform for pump_events"
#endif

namespace almondnamespace::platform 
{
    inline bool pump_events()  // Returns false if the user closed the window / requested quit
    {
#if defined(_WIN32)
#ifndef ALMOND_MAIN_HEADLESS
#ifndef ALMOND_USING_RAYLIB

        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                return false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // 🔑 After dispatching, update global input states
     //   almondnamespace::input::poll_input();

        return true;

#endif // !ALMOND_USING_RAYLIB
#endif // !ALMOND_MAIN_HEADLESS

#elif defined(__APPLE__) && defined(__MACH__)
        @autoreleasepool{
            NSEvent * ev = nil;
            while ((ev = [NSApp nextEventMatchingMask : NSEventMaskAny
                                             untilDate : [NSDate distantPast]
                                                inMode : NSDefaultRunLoopMode
                                               dequeue : YES])) {
                if (ev.type == NSEventTypeApplicationDefined /* or your quit type */)
                    return false;
                [NSApp sendEvent : ev] ;
            }
        }

        almondnamespace::input::poll_input(); // macOS stub version

        return true;
    
#elif defined(__linux__)
        //namespace almondnamespace::platform {
            extern Display* global_display;
            extern ::Window global_window;
        //}

        //using almondnamespace::platform::global_display;
        //using almondnamespace::platform::global_window;

        namespace core = almondnamespace::core;

        if (global_display)
        {
            almondnamespace::input::poll_input(global_display, global_window);
        }
        else
        {
            almondnamespace::input::poll_input(nullptr, 0);
        }

        bool keepRunning = true;

        while (global_display && XPending(global_display)) {
            XEvent ev;
            XNextEvent(global_display, &ev);

            switch (ev.type)
            {
            case ClientMessage:
                keepRunning = false;
                break;

            case KeyPress:
            {
                KeySym sym = XLookupKeysym(&ev.xkey, 0);
                auto key = almondnamespace::input::map_keysym_to_key(sym);
                almondnamespace::input::handle_key_event(key, true);
                break;
            }
            case KeyRelease:
            {
                KeySym sym = XLookupKeysym(&ev.xkey, 0);
                auto key = almondnamespace::input::map_keysym_to_key(sym);
                almondnamespace::input::handle_key_event(key, false);
                break;
            }

            case ButtonPress:
            {
                almondnamespace::input::handle_mouse_motion(ev.xbutton.x, ev.xbutton.y, false);

                switch (ev.xbutton.button)
                {
                case Button4:
                    almondnamespace::input::handle_mouse_wheel(1);
                    break;
                case Button5:
                    almondnamespace::input::handle_mouse_wheel(-1);
                    break;
#ifdef Button6
                case Button6:
#endif
#ifdef Button7
                case Button7:
#endif
                    break; // Horizontal scroll not yet surfaced
                default:
                {
                    auto btn = almondnamespace::input::map_button_to_mousebutton(ev.xbutton.button);
                    almondnamespace::input::handle_mouse_button_event(btn, true);
                    break;
                }
                }
                break;
            }
            case ButtonRelease:
            {
                almondnamespace::input::handle_mouse_motion(ev.xbutton.x, ev.xbutton.y, false);

                auto btn = almondnamespace::input::map_button_to_mousebutton(ev.xbutton.button);
                almondnamespace::input::handle_mouse_button_event(btn, false);
                break;
            }

            case MotionNotify:
                almondnamespace::input::handle_mouse_motion(ev.xmotion.x, ev.xmotion.y, false);
                break;

            case EnterNotify:
                global_window = ev.xcrossing.window;
                almondnamespace::input::handle_mouse_motion(ev.xcrossing.x, ev.xcrossing.y, false);
                break;

            case LeaveNotify:
                if (global_window == ev.xcrossing.window)
                {
                    almondnamespace::input::handle_mouse_motion(-1, -1, false);
                }
                break;

            case FocusIn:
                global_window = ev.xfocus.window;
                break;

            case FocusOut:
                if (global_window == ev.xfocus.window)
                {
                    global_window = 0;
                }
                break;

            case DestroyNotify:
                if (global_window == ev.xdestroywindow.window)
                {
                    global_window = 0;
                }
                break;

            case ConfigureNotify:
                core::HandleX11Configure(ev.xconfigure.window, ev.xconfigure.width, ev.xconfigure.height);
                break;

            case MappingNotify:
                XRefreshKeyboardMapping(&ev.xmapping);
                break;

            default:
                break;
            }
        }

        return keepRunning;

#elif defined(__ANDROID__)
        extern struct android_app* global_android_app;
        int events;
        struct android_poll_source* source;
        while (ALooper_pollAll(0, nullptr, &events, (void**)&source) >= 0) {
            if (source) source->process(global_android_app, source);
        }
        // TODO: integrate Android key/touch input → almondnamespace::input
        return !global_android_app->destroyRequested;

#elif defined(__EMSCRIPTEN__)
        // EMSCRIPTEN uses callback hooks; nothing to poll here
        return true;
#endif

        return true;  // Fallback
    }

} // namespace almondnamespace::platform