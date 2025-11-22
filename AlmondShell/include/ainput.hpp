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
 // ainput.hpp
#pragma once

#include "aplatform.hpp"   // must always come first

#include "acontextwindow.hpp"
#include "acontextstate.hpp"

// Include platform-specific headers
#if defined(_WIN32)
    #include "aframework.hpp"
#elif defined(__APPLE__)
    #include <ApplicationServices/ApplicationServices.h>
#elif defined(__linux__)
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #include <X11/keysym.h>
    #include <X11/extensions/XInput2.h>
#endif

//STL
#include <iostream>
#include <cstdint>
#include <bitset>
#include <atomic>
#include <thread>
#include <array>
#include <mutex>
#include <shared_mutex>

namespace almondnamespace::input // Input namespace
{
    enum Key : uint16_t // Key enum
    {
        Unknown = 0,
        A, B, C, D, E, F, G, H, I, J,
        K, L, M, N, O, P, Q, R, S, T,
        U, V, W, X, Y, Z,
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        Space, Apostrophe, Comma, Minus, Period, Slash,
        Semicolon, Equal, LeftBracket, Backslash, RightBracket, GraveAccent,
        Escape, Enter, Tab, Backspace, Insert, Delete,
        Right, Left, Down, Up,
        PageUp, PageDown, Home, End,
        CapsLock, ScrollLock, NumLock,
        PrintScreen, Pause,
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10,
        F11, F12, F13, F14, F15, F16, F17, F18, F19, F20,
        F21, F22, F23, F24,
        LeftShift, RightShift,
        LeftControl, RightControl,
        LeftAlt, RightAlt,
        LeftSuper, RightSuper,
        Menu,
        KP0, KP1, KP2, KP3, KP4, KP5, KP6, KP7, KP8, KP9,
        KPDecimal, KPDivide, KPMultiply,
        KPSubtract, KPAdd, KPEnter, KPEqual,
        Count
    };

    enum MouseButton : std::uint8_t // Mouse buttons
    {
        MouseLeft = 0,
        MouseRight,
        MouseMiddle,
        MouseButton4,
        MouseButton5,
        MouseButton6,
        MouseButton7,
        MouseButton8,
        MouseCount
    };

    // --- State Storage ---
    inline std::thread::id g_pollingThread{};
    inline std::atomic<bool> g_pollingThreadLocked{ false };

    inline std::shared_mutex g_inputMutex{};

    inline std::bitset<Key::Count> keyDown{};
    inline std::bitset<Key::Count> keyPressed{};

    inline std::bitset<MouseButton::MouseCount> mouseDown{};
    inline std::bitset<MouseButton::MouseCount> mousePressed{};

    inline std::atomic<int> mouseX{ 0 };
    inline std::atomic<int> mouseY{ 0 };
    inline std::atomic<int> mouseWheel{ 0 };
    inline std::atomic<bool> mouseCoordsAreGlobal{ true };

    inline void set_mouse_coords_are_global(bool value) {
        mouseCoordsAreGlobal.store(value, std::memory_order_release);
    }

    inline bool are_mouse_coords_global() {
        return mouseCoordsAreGlobal.load(std::memory_order_acquire);
    }

    inline void designate_polling_thread(std::thread::id id) {
        g_pollingThread = id;
        g_pollingThreadLocked.store(true, std::memory_order_release);
    }

    inline void designate_polling_thread_to_current() {
        designate_polling_thread(std::this_thread::get_id());
    }

    inline void clear_polling_thread_designation() {
        g_pollingThread = std::thread::id{};
        g_pollingThreadLocked.store(false, std::memory_order_release);
    }

    inline bool can_poll_on_this_thread() {
        if (!g_pollingThreadLocked.load(std::memory_order_acquire)) {
            return true;
        }
        return std::this_thread::get_id() == g_pollingThread;
    }

    // --- Platform-specific helpers ---
#if defined(_WIN32)
        // Get mouse pos relative to client area; returns -1,-1 if outside
    inline void get_mouse_position(int& x, int& y)
    {
#ifndef ALMOND_MAIN_HEADLESS
#ifndef ALMOND_USING_RAYLIB

        static HWND s_hwnd = almondnamespace::contextwindow::WindowData::getWindowHandle();
        if (!s_hwnd) {
            x = -1;
            y = -1;
            return;
        }
        POINT pt;
        if (GetCursorPos(&pt)) {
            POINT clientPt = pt;
            ScreenToClient(s_hwnd, &clientPt);
            RECT rc;
            GetClientRect(s_hwnd, &rc);
            if (clientPt.x < rc.left || clientPt.x >= rc.right ||
                clientPt.y < rc.top || clientPt.y >= rc.bottom) {
                x = -1;
                y = -1;
            }
            else {
                x = clientPt.x;
                y = clientPt.y;
            }
        }
        else {
            x = -1;
            y = -1;
        }
#endif // ALMOND_USING_RAYLIB
#endif // ALMOND_MAIN_HEADLESS
    }

    inline constexpr int map_key_to_vk(Key k) // Map Key to Win32 VK codes
    {
        switch (k)
        {
            case Key::A: return 'A'; case Key::B: return 'B'; case Key::C: return 'C'; case Key::D: return 'D';
            case Key::E: return 'E'; case Key::F: return 'F'; case Key::G: return 'G'; case Key::H: return 'H';
            case Key::I: return 'I'; case Key::J: return 'J'; case Key::K: return 'K'; case Key::L: return 'L';
            case Key::M: return 'M'; case Key::N: return 'N'; case Key::O: return 'O'; case Key::P: return 'P';
            case Key::Q: return 'Q'; case Key::R: return 'R'; case Key::S: return 'S'; case Key::T: return 'T';
            case Key::U: return 'U'; case Key::V: return 'V'; case Key::W: return 'W'; case Key::X: return 'X';
            case Key::Y: return 'Y'; case Key::Z: return 'Z';

            case Key::Num0: return '0'; case Key::Num1: return '1'; case Key::Num2: return '2'; case Key::Num3: return '3'; case Key::Num4: return '4';
            case Key::Num5: return '5'; case Key::Num6: return '6'; case Key::Num7: return '7'; case Key::Num8: return '8'; case Key::Num9: return '9';

#ifndef ALMOND_MAIN_HEADLESS
#ifndef ALMOND_USING_RAYLIB
            case Key::Escape: return VK_ESCAPE;
            case Key::Enter: return VK_RETURN;
            case Key::Tab: return VK_TAB;
            case Key::Backspace: return VK_BACK;
            case Key::Space: return VK_SPACE;
            case Key::Left: return VK_LEFT;
            case Key::Right: return VK_RIGHT;
            case Key::Up: return VK_UP;
            case Key::Down: return VK_DOWN;
            case Key::Insert: return VK_INSERT;
            case Key::Delete: return VK_DELETE;
            case Key::Home: return VK_HOME;
            case Key::End: return VK_END;
            case Key::PageUp: return VK_PRIOR;
            case Key::PageDown: return VK_NEXT;
            case Key::CapsLock: return VK_CAPITAL;
            case Key::ScrollLock: return VK_SCROLL;
            case Key::NumLock: return VK_NUMLOCK;
            case Key::PrintScreen: return VK_SNAPSHOT;
            case Key::Pause: return VK_PAUSE;

            case Key::F1: return VK_F1; case Key::F2: return VK_F2; case Key::F3: return VK_F3; case Key::F4: return VK_F4;
            case Key::F5: return VK_F5; case Key::F6: return VK_F6; case Key::F7: return VK_F7; case Key::F8: return VK_F8;
            case Key::F9: return VK_F9; case Key::F10: return VK_F10; case Key::F11: return VK_F11; case Key::F12: return VK_F12;
            case Key::F13: return 0x7C; case Key::F14: return 0x7D; case Key::F15: return 0x7E; case Key::F16: return 0x7F;
            case Key::F17: return 0x80; case Key::F18: return 0x81; case Key::F19: return 0x82; case Key::F20: return 0x83;
            case Key::F21: return 0x84; case Key::F22: return 0x85; case Key::F23: return 0x86; case Key::F24: return 0x87;

            case Key::LeftShift: return VK_LSHIFT;
            case Key::RightShift: return VK_RSHIFT;
            case Key::LeftControl: return VK_LCONTROL;
            case Key::RightControl: return VK_RCONTROL;
            case Key::LeftAlt: return VK_LMENU;
            case Key::RightAlt: return VK_RMENU;
            case Key::LeftSuper: return VK_LWIN;
            case Key::RightSuper: return VK_RWIN;
            case Key::Menu: return VK_APPS;

            case Key::KP0: return VK_NUMPAD0; case Key::KP1: return VK_NUMPAD1; case Key::KP2: return VK_NUMPAD2; case Key::KP3: return VK_NUMPAD3; case Key::KP4: return VK_NUMPAD4;
            case Key::KP5: return VK_NUMPAD5; case Key::KP6: return VK_NUMPAD6; case Key::KP7: return VK_NUMPAD7; case Key::KP8: return VK_NUMPAD8; case Key::KP9: return VK_NUMPAD9;
            case Key::KPDecimal: return VK_DECIMAL;
            case Key::KPDivide: return VK_DIVIDE;
            case Key::KPMultiply: return VK_MULTIPLY;
            case Key::KPSubtract: return VK_SUBTRACT;
            case Key::KPAdd: return VK_ADD;


            case Key::KPEnter: return VK_RETURN; // no distinct VK for KP Enter, reuse return
            case Key::KPEqual: return 0; // No direct VK code for keypad equal
#endif // ALMOND_USING_RAYLIB
#endif  // ALMOND_MAIN_HEADLESS

            default: return 0;
        }
    }

#ifndef ALMOND_MAIN_HEADLESS
#ifndef ALMOND_USING_RAYLIB

    inline constexpr int map_mouse_to_vk(MouseButton btn) // Map MouseButton to Win32 VK
    {
        switch (btn)  // switch - Mouse Buttons
        {
            case MouseButton::MouseLeft: return VK_LBUTTON;  // Left - Mouse Button
            case MouseButton::MouseRight: return VK_RBUTTON;  // Right - Mouse Button
            case MouseButton::MouseMiddle: return VK_MBUTTON;  // Middle - Mouse Button
            case MouseButton::MouseButton4: return XBUTTON1;  // Button4 - Mouse Button
            case MouseButton::MouseButton5: return XBUTTON2;  // Button5 - Mouse Button
            default: return 0; // No VK codes for buttons 6-8, treat as unknown
        }
    }
#endif  // ALMOND_USING_RAYLIB

#endif // ALMOND_MAIN_HEADLESS

    inline void update_input()  // Update input states - must be called each frame
    {
        std::unique_lock<std::shared_mutex> lock(g_inputMutex);

        // Reset pressed states
        keyPressed.reset();
        mousePressed.reset();

#ifndef ALMOND_MAIN_HEADLESS
#ifndef ALMOND_USING_RAYLIB

        // Update keys
        for (int i = 0; i < Key::Count; ++i) {
            int vk = map_key_to_vk(static_cast<Key>(i));
            if (vk == 0) continue;
            SHORT state = GetAsyncKeyState(vk);
            bool isDownNow = (state & 0x8000) != 0;

            if (isDownNow && !keyDown[i]) {
                keyPressed.set(i);
            }
            keyDown.set(i, isDownNow);
        }

        // Update mouse buttons
        for (int i = 0; i < MouseButton::MouseCount; ++i) {
            int vk = map_mouse_to_vk(static_cast<MouseButton>(i));
            if (vk == 0) continue;
            SHORT state = GetAsyncKeyState(vk);
            bool isDownNow = (state & 0x8000) != 0;

            if (isDownNow && !mouseDown[i]) {
                mousePressed.set(i);
            }
            mouseDown.set(i, isDownNow);
        }

        // Get cursor position relative to the active window (assumes HWND is active window)
        POINT p;
        if (GetCursorPos(&p)) {
            mouseX.store(p.x, std::memory_order_relaxed);
            mouseY.store(p.y, std::memory_order_relaxed);
        }
        set_mouse_coords_are_global(true);
#endif // ALMOND_USING_RAYLIB

#endif // ALMOND_MAIN_HEADLESS
        // Mouse wheel: Requires Windows message handling, leave zero here
        mouseWheel.store(0, std::memory_order_relaxed);
    }

#elif defined(__APPLE__)

// Map Key enum to macOS virtual key codes (using Quartz keycodes)
// Map Key enum to macOS virtual key codes (Quartz keycodes)
inline constexpr uint16_t map_key_to_mac(Key k)
{
    switch (k)
    {
        case Key::A: return 0x00; case Key::S: return 0x01; case Key::D: return 0x02; case Key::F: return 0x03;
        case Key::H: return 0x04; case Key::G: return 0x05; case Key::Z: return 0x06; case Key::X: return 0x07;
        case Key::C: return 0x08; case Key::V: return 0x09; case Key::B: return 0x0B; case Key::Q: return 0x0C;
        case Key::W: return 0x0D; case Key::E: return 0x0E; case Key::R: return 0x0F; case Key::Y: return 0x10;
        case Key::T: return 0x11;
    
        case Key::Num1: return 0x12; case Key::Num2: return 0x13; case Key::Num3: return 0x14;
        case Key::Num4: return 0x15; case Key::Num6: return 0x16; case Key::Num5: return 0x17; case Key::Equal: return 0x18;
        case Key::Num9: return 0x19; case Key::Num7: return 0x1A; case Key::Minus: return 0x1B; case Key::Num8: return 0x1C;
        case Key::Num0: return 0x1D; case Key::RightBracket: return 0x1E; case Key::O: return 0x1F; case Key::U: return 0x20;
        case Key::LeftBracket: return 0x21; case Key::I: return 0x22; case Key::P: return 0x23; case Key::Enter: return 0x24;
        case Key::L: return 0x25; case Key::J: return 0x26; case Key::Apostrophe: return 0x27; case Key::K: return 0x28;
        case Key::Semicolon: return 0x29; case Key::Backslash: return 0x2A; case Key::Comma: return 0x2B;
        case Key::Slash: return 0x2C; case Key::N: return 0x2D; case Key::M: return 0x2E; case Key::Period: return 0x2F;
        case Key::Tab: return 0x30; case Key::Space: return 0x31; case Key::Backquote: return 0x32;
        case Key::Delete: return 0x33; case Key::Escape: return 0x35;
        case Key::RightCommand: return 0x36; case Key::LeftCommand: return 0x37;
        case Key::LeftShift: return 0x38; case Key::CapsLock: return 0x39; case Key::LeftAlt: return 0x3A;
        case Key::LeftControl: return 0x3B; case Key::RightShift: return 0x3C; case Key::RightAlt: return 0x3D;
        case Key::RightControl: return 0x3E; case Key::Function: return 0x3F; case Key::F17: return 0x40;
        case Key::VolumeUp: return 0x48; case Key::VolumeDown: return 0x49; case Key::Mute: return 0x4A;
        case Key::F18: return 0x4F; case Key::F19: return 0x50; case Key::F20: return 0x5A;
        case Key::F5: return 0x60; case Key::F6: return 0x61; case Key::F7: return 0x62; case Key::F3: return 0x63;
        case Key::F8: return 0x64; case Key::F9: return 0x65; case Key::F11: return 0x67; case Key::F13: return 0x69;
        case Key::F16: return 0x6A; case Key::F14: return 0x6B; case Key::F10: return 0x6D; case Key::F12: return 0x6F;
        case Key::F15: return 0x71; case Key::Help: return 0x72; case Key::Home: return 0x73; case Key::PageUp: return 0x74;
        case Key::DeleteForward: return 0x75; case Key::F4: return 0x76; case Key::End: return 0x77; case Key::F2: return 0x78;
        case Key::PageDown: return 0x79; case Key::F1: return 0x7A; case Key::Left: return 0x7B; case Key::Right: return 0x7C;
        case Key::Down: return 0x7D; case Key::Up: return 0x7E;

        default: return 0xFFFF; // invalid/unknown
    }
}

#elif defined(__linux__)

inline constexpr uint16_t map_key_to_linux(Key k) {
    switch (k)
    {
        case Key::A: return 38; case Key::B: return 56; case Key::C: return 54; case Key::D: return 40;
        case Key::E: return 26; case Key::F: return 41; case Key::G: return 42; case Key::H: return 43;
        case Key::I: return 31; case Key::J: return 44; case Key::K: return 45; case Key::L: return 46;
        case Key::M: return 58; case Key::N: return 57; case Key::O: return 32; case Key::P: return 33;
        case Key::Q: return 24; case Key::R: return 27; case Key::S: return 39; case Key::T: return 28;
        case Key::U: return 30; case Key::V: return 55; case Key::W: return 25; case Key::X: return 53;
        case Key::Y: return 29; case Key::Z: return 52;

        case Key::Num1: return 10; case Key::Num2: return 11; case Key::Num3: return 12; case Key::Num4: return 13;
        case Key::Num5: return 14; case Key::Num6: return 15; case Key::Num7: return 16; case Key::Num8: return 17;
        case Key::Num9: return 18; case Key::Num0: return 19;

        case Key::Enter: return 36; case Key::Escape: return 9; case Key::Backspace: return 22; case Key::Tab: return 23;
        case Key::Space: return 65; case Key::Minus: return 20; case Key::Equal: return 21; case Key::LeftBracket: return 34;
        case Key::RightBracket: return 35; case Key::Backslash: return 51; case Key::Semicolon: return 47; case Key::Apostrophe: return 48;
        case Key::GraveAccent: return 49; case Key::Comma: return 59; case Key::Period: return 60; case Key::Slash: return 61;

        case Key::CapsLock: return 66; case Key::F1: return 67; case Key::F2: return 68; case Key::F3: return 69; case Key::F4: return 70;
        case Key::F5: return 71; case Key::F6: return 72; case Key::F7: return 73; case Key::F8: return 74; case Key::F9: return 75;
        case Key::F10: return 76; case Key::F11: return 95; case Key::F12: return 96;

        case Key::Insert: return 118; case Key::Delete: return 119; case Key::Home: return 110; case Key::End: return 115;
        case Key::PageUp: return 112; case Key::PageDown: return 117;

        case Key::Left: return 113; case Key::Right: return 114; case Key::Up: return 111; case Key::Down: return 116;

        case Key::NumLock: return 77; case Key::ScrollLock: return 78; case Key::Pause: return 127;

        case Key::LeftShift: return 50; case Key::RightShift: return 62;
        case Key::LeftControl: return 37; case Key::RightControl: return 105;
        case Key::LeftAlt: return 64; case Key::RightAlt: return 108;

        case Key::PrintScreen: return 107;

        default: return 0; // unknown
    }
}
#endif

#if defined(_WIN32)
// Poll and update input states on Win32
inline void poll_input()
{
    if (!can_poll_on_this_thread()) {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(g_inputMutex);

    // Reset pressed states; pressed is "went down this frame"
    keyPressed.reset();
    mousePressed.reset();
    mouseWheel.store(0, std::memory_order_relaxed);

#ifndef ALMOND_MAIN_HEADLESS
#ifndef ALMOND_USING_RAYLIB

    // Query keyboard state
    for (uint16_t k = 1; k < Key::Count; ++k) {
        int vk = map_key_to_vk(static_cast<Key>(k));
        if (vk == 0) continue;

        SHORT keyState = GetAsyncKeyState(vk);
        bool isDown = (keyState & 0x8000) != 0;

        if (isDown && !keyDown.test(k)) {
            // Key was just pressed
            keyPressed.set(k);
        }
        keyDown.set(k, isDown);
    }

    // Mouse buttons (Left=VK_LBUTTON=0x01, Right=VK_RBUTTON=0x02, Middle=VK_MBUTTON=0x04)
    constexpr int mouseVK[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
    for (uint8_t btn = 0; btn < MouseButton::MouseCount; ++btn) {
        const int vk = (btn < std::size(mouseVK)) ? mouseVK[btn] : 0;
        const SHORT state = vk ? GetAsyncKeyState(vk) : 0;
        const bool isDown = (state & 0x8000) != 0;

        if (isDown && !mouseDown.test(btn)) {
            mousePressed.set(btn);
        }
        mouseDown.set(btn, isDown);
    }

    // Update mouse position (screen coordinates)
    POINT pt;
    if (GetCursorPos(&pt)) {
        mouseX.store(pt.x, std::memory_order_relaxed);
        mouseY.store(pt.y, std::memory_order_relaxed);
    }
    set_mouse_coords_are_global(true);

    // Mouse wheel: typically requires processing WM_MOUSEWHEEL in window proc,
    // but here you might want to reset or accumulate wheel delta externally.
    // Leaving it zero here; integrate with your message pump as needed.
#endif // ALMOND_USING_RAYLIB

#endif // ALMOND_MAIN_HEADLESS
}

#elif defined(__APPLE__)
// map keys to almondnamespace::input::Key enums,
// update bitsets accordingly.
inline void poll_input() // macOS input polling
{
    if (!can_poll_on_this_thread()) {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(g_inputMutex);

    previous_keys = current_keys;
    previous_mouse = current_mouse;

    keyPressed.reset();
    mousePressed.reset();
    mouseWheel.store(0, std::memory_order_relaxed);

    // Fetch current keyboard state using Quartz API
    // Here we fake it with CGEventSourceKeyState; ideally, you'd hook event taps

    for (int k = 0; k < static_cast<int>(Key::Count); ++k) {
        uint16_t macKeyCode = map_key_to_mac(static_cast<Key>(k));
        if (macKeyCode == UINT16_MAX) continue;  // invalid keycode
        bool down = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, macKeyCode);
        keyPressed[k] = down && !keyDown[k];
        keyDown[k] = down;
    }

    // Mouse buttons
    // CGEventSourceButtonState can be used for buttons 1-5
    for (int b = 0; b < static_cast<int>(MouseButton::Count); ++b) {
        bool down = CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, b + 1);
        mousePressed[b] = down && !mouseDown[b];
        mouseDown[b] = down;
    }

    // Get mouse position (in global screen coords)
    CGEventRef event = CGEventCreate(NULL);
    CGPoint loc = CGEventGetLocation(event);
    mouseX.store(static_cast<int>(loc.x), std::memory_order_relaxed);
    mouseY.store(static_cast<int>(loc.y), std::memory_order_relaxed);
    set_mouse_coords_are_global(true);
    CFRelease(event);
}

#elif defined(__linux__)

namespace almondnamespace::platform {
    extern Display* global_display;
    extern ::Window global_window;
}

inline void get_mouse_position(int& x, int& y)
{
    Display* display = almondnamespace::platform::global_display;
    ::Window window = almondnamespace::platform::global_window;

    if (!display || window == 0)
    {
        x = -1;
        y = -1;
        return;
    }

    Window root{};
    Window child{};
    int rootX = 0;
    int rootY = 0;
    int winX = 0;
    int winY = 0;
    unsigned int mask = 0;

    if (XQueryPointer(display, window, &root, &child, &rootX, &rootY, &winX, &winY, &mask))
    {
        XWindowAttributes attrs{};
        if (XGetWindowAttributes(display, window, &attrs))
        {
            if (winX < 0 || winY < 0 || winX >= attrs.width || winY >= attrs.height)
            {
                x = -1;
                y = -1;
            }
            else
            {
                x = winX;
                y = winY;
            }
        }
        else
        {
            x = winX;
            y = winY;
        }

        mouseX.store(x, std::memory_order_relaxed);
        mouseY.store(y, std::memory_order_relaxed);
        set_mouse_coords_are_global(false);
    }
    else
    {
        x = -1;
        y = -1;
    }
}

inline Key map_keysym_to_key(KeySym sym) noexcept
{
    if (sym >= XK_a && sym <= XK_z)
    {
        return static_cast<Key>(Key::A + (sym - XK_a));
    }
    if (sym >= XK_A && sym <= XK_Z)
    {
        return static_cast<Key>(Key::A + (sym - XK_A));
    }
    if (sym >= XK_0 && sym <= XK_9)
    {
        return static_cast<Key>(Key::Num0 + (sym - XK_0));
    }

    if (sym >= XK_F1 && sym <= XK_F24)
    {
        return static_cast<Key>(Key::F1 + (sym - XK_F1));
    }

    if (sym >= XK_KP_0 && sym <= XK_KP_9)
    {
        return static_cast<Key>(Key::KP0 + (sym - XK_KP_0));
    }

    switch (sym)
    {
    case XK_space: return Key::Space;
    case XK_apostrophe: return Key::Apostrophe;
    case XK_comma: return Key::Comma;
    case XK_minus: return Key::Minus;
    case XK_period: return Key::Period;
    case XK_slash: return Key::Slash;
    case XK_semicolon: return Key::Semicolon;
    case XK_equal: return Key::Equal;
    case XK_bracketleft: return Key::LeftBracket;
    case XK_backslash: return Key::Backslash;
    case XK_bracketright: return Key::RightBracket;
    case XK_grave: return Key::GraveAccent;

    case XK_Escape: return Key::Escape;
    case XK_Return: return Key::Enter;
    case XK_Tab:
    case XK_ISO_Left_Tab: return Key::Tab;
    case XK_BackSpace: return Key::Backspace;

    case XK_Insert: return Key::Insert;
    case XK_Delete: return Key::Delete;
    case XK_Home: return Key::Home;
    case XK_End: return Key::End;
    case XK_Page_Up: return Key::PageUp;
    case XK_Page_Down: return Key::PageDown;

    case XK_Left: return Key::Left;
    case XK_Right: return Key::Right;
    case XK_Up: return Key::Up;
    case XK_Down: return Key::Down;

    case XK_Caps_Lock: return Key::CapsLock;
    case XK_Num_Lock: return Key::NumLock;
    case XK_Scroll_Lock: return Key::ScrollLock;
    case XK_Print: return Key::PrintScreen;
    case XK_Pause: return Key::Pause;

    case XK_Shift_L: return Key::LeftShift;
    case XK_Shift_R: return Key::RightShift;
    case XK_Control_L: return Key::LeftControl;
    case XK_Control_R: return Key::RightControl;
    case XK_Alt_L: return Key::LeftAlt;
    case XK_Alt_R:
    case XK_ISO_Level3_Shift: return Key::RightAlt;
    case XK_Super_L:
    case XK_Meta_L: return Key::LeftSuper;
    case XK_Super_R:
    case XK_Meta_R: return Key::RightSuper;
    case XK_Menu: return Key::Menu;

    case XK_KP_Decimal: return Key::KPDecimal;
    case XK_KP_Divide: return Key::KPDivide;
    case XK_KP_Multiply: return Key::KPMultiply;
    case XK_KP_Subtract: return Key::KPSubtract;
    case XK_KP_Add: return Key::KPAdd;
    case XK_KP_Enter: return Key::KPEnter;
    case XK_KP_Equal: return Key::KPEqual;

    default:
        return Key::Unknown;
    }
}

inline MouseButton map_button_to_mousebutton(unsigned int button) noexcept
{
    switch (button)
    {
    case Button1: return MouseButton::MouseLeft;
    case Button3: return MouseButton::MouseRight;
    case Button2: return MouseButton::MouseMiddle;
    case 8: return MouseButton::MouseButton4;
    case 9: return MouseButton::MouseButton5;
    case 10: return MouseButton::MouseButton6;
    case 11: return MouseButton::MouseButton7;
    case 12: return MouseButton::MouseButton8;
    default: return MouseButton::MouseCount;
    }
}

inline void handle_key_event(Key key, bool pressed)
{
    if (key == Key::Unknown)
    {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(g_inputMutex);
    const size_t idx = static_cast<size_t>(key);
    const bool wasDown = keyDown.test(idx);
    keyDown.set(idx, pressed);
    if (pressed && !wasDown)
    {
        keyPressed.set(idx);
    }
    else if (!pressed)
    {
        keyPressed.reset(idx);
    }
}

inline void handle_mouse_button_event(MouseButton button, bool pressed)
{
    if (button == MouseButton::MouseCount)
    {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(g_inputMutex);
    const size_t idx = static_cast<size_t>(button);
    const bool wasDown = mouseDown.test(idx);
    mouseDown.set(idx, pressed);
    if (pressed && !wasDown)
    {
        mousePressed.set(idx);
    }
    else if (!pressed)
    {
        mousePressed.reset(idx);
    }
}

inline void handle_mouse_motion(int x, int y, bool coordsAreGlobal)
{
    {
        std::unique_lock<std::shared_mutex> lock(g_inputMutex);
        mouseX.store(x, std::memory_order_relaxed);
        mouseY.store(y, std::memory_order_relaxed);
    }
    set_mouse_coords_are_global(coordsAreGlobal);
}

inline void handle_mouse_wheel(int delta)
{
    mouseWheel.fetch_add(delta, std::memory_order_relaxed);
}

inline void poll_input(Display* /*display*/, ::Window /*window*/)
{
    if (!can_poll_on_this_thread())
    {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(g_inputMutex);
    keyPressed.reset();
    mousePressed.reset();
    mouseWheel.store(0, std::memory_order_relaxed);
}
#endif

inline bool is_key_held(Key k) {
    std::shared_lock<std::shared_mutex> lock(g_inputMutex);
    return keyDown.test(static_cast<size_t>(k));
}

inline bool is_key_down(Key k) {
    std::shared_lock<std::shared_mutex> lock(g_inputMutex);
    return keyPressed.test(static_cast<size_t>(k));
}

inline bool is_mouse_button_held(MouseButton btn) {
    std::shared_lock<std::shared_mutex> lock(g_inputMutex);
    return mouseDown.test(static_cast<size_t>(btn));
}

inline bool is_mouse_button_down(MouseButton btn) {
    std::shared_lock<std::shared_mutex> lock(g_inputMutex);
    return mousePressed.test(static_cast<size_t>(btn));
}

} 

//winproc
#if defined(_WIN32)
#ifdef ALMOND_USING_SDL
using almondnamespace::sdlcontext::state::s_sdlstate;
#endif
#ifdef ALMOND_USING_SFML
using almondnamespace::sfmlcontext::state::s_sfmlstate;
#endif
#ifdef ALMOND_USING_RAYLIB
using almondnamespace::raylibcontext::s_raylibstate;
#endif
#ifdef ALMOND_USING_OPENGL
using almondnamespace::openglstate::s_openglstate;
#endif
#ifdef ALMOND_USING_VULKAN
using almondnamespace::vulkancontext::s_vulkanstate;
#endif

#ifndef ALMOND_MAIN_HEADLESS
#ifndef ALMOND_USING_RAYLIB

inline void safe_log(const char* msg) noexcept {
    OutputDebugStringA(msg);
    std::cout << msg;
}

inline void safe_log(const std::string& msg) noexcept {
    OutputDebugStringA(msg.c_str());
    std::cout << msg;
}

inline void safe_log_number(UINT msg) noexcept {
    char buf[128];
    int len = snprintf(buf, sizeof(buf), "[WndProc] Unhandled message: %u\n", msg);
    if (len > 0) {
        OutputDebugStringA(buf);
        std::cout << buf;
    }
}


inline LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
    try {
        auto log_mouse = [](const char* btn, const char* state) {
#if defined(DEBUG_INPUT)
            safe_log(std::string("[Input] Mouse Button ") + btn + state + "\n");
#endif
            };

        auto log_key = [](WPARAM key, const char* state) {
#if defined(DEBUG_INPUT)
            safe_log("[Input] Key Code " + std::to_string(key) + " " + state + "\n");
#endif
            };

        auto log_char = [](WPARAM key) {
#if defined(DEBUG_INPUT)
            if (key >= 32 && key < 127) {
                safe_log("[Input] Character Code '" + std::string(1, static_cast<char>(key)) +
                    "' (" + std::to_string(key) + ")\n");
            }
            else {
                safe_log("[Input] CHAR (code " + std::to_string(key) + ") [non-printable]\n");
            }
#endif
            };

        auto log_move = [](int x, int y) {
#if defined(DEBUG_MOUSE_MOVEMENT)
            safe_log("[Input] Mouse MOVE to (" + std::to_string(x) + ", " + std::to_string(y) + ")\n");
#endif
            };

        switch (msg) {

            // ---------- Mouse Buttons ----------
        case WM_LBUTTONDOWN:
            almondnamespace::input::mouseDown.set(static_cast<size_t>(almondnamespace::input::MouseButton::MouseLeft), true);
            log_mouse("Left", "DOWN");
            break;
        case WM_LBUTTONUP:
            almondnamespace::input::mouseDown.set(static_cast<size_t>(almondnamespace::input::MouseButton::MouseLeft), false);
            log_mouse("Left", "UP");
            break;
        case WM_RBUTTONDOWN:
            almondnamespace::input::mouseDown.set(static_cast<size_t>(almondnamespace::input::MouseButton::MouseRight), true);
            log_mouse("Right", "DOWN");
            break;
        case WM_RBUTTONUP:
            almondnamespace::input::mouseDown.set(static_cast<size_t>(almondnamespace::input::MouseButton::MouseRight), false);
            log_mouse("Right", "UP");
            break;
        case WM_MBUTTONDOWN:
            almondnamespace::input::mouseDown.set(static_cast<size_t>(almondnamespace::input::MouseButton::MouseMiddle), true);
            log_mouse("Middle", "DOWN");
            break;
        case WM_MBUTTONUP:
            almondnamespace::input::mouseDown.set(static_cast<size_t>(almondnamespace::input::MouseButton::MouseMiddle), false);
            log_mouse("Middle", "UP");
            break;

        case WM_XBUTTONDOWN: {
            int idx = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ?
                static_cast<int>(almondnamespace::input::MouseButton::MouseButton4) :
                static_cast<int>(almondnamespace::input::MouseButton::MouseButton5);
            almondnamespace::input::mouseDown.set(idx, true);
            log_mouse(idx == static_cast<int>(almondnamespace::input::MouseButton::MouseButton4) ? "X1" : "X2", "DOWN");
            break;
        }
        case WM_XBUTTONUP: {
            int idx = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ?
                static_cast<int>(almondnamespace::input::MouseButton::MouseButton4) :
                static_cast<int>(almondnamespace::input::MouseButton::MouseButton5);
            almondnamespace::input::mouseDown.set(idx, false);
            log_mouse(idx == static_cast<int>(almondnamespace::input::MouseButton::MouseButton4) ? "X1" : "X2", "UP");
            break;
        }

                         // ---------- Mouse Move ----------
        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
            log_move(x, y);
            break;
        }

                         // ---------- Keyboard ----------
        case WM_KEYDOWN:
            if (wParam < 256)
                almondnamespace::input::keyDown.set(static_cast<size_t>(wParam), true);
            log_key(wParam, "DOWN");
            break;

        case WM_KEYUP:
            if (wParam < 256)
                almondnamespace::input::keyDown.set(static_cast<size_t>(wParam), false);
            log_key(wParam, "UP");
            break;

        case WM_CHAR:
            log_char(wParam);
            break;

            // ---------- Non-client mouse move ----------
        case WM_NCMOUSEMOVE:
#if defined(DEBUG_INPUT_OS)
            safe_log("[WndProc] Non-client mouse move\n");
#endif
            break;

            // ---------- Set Cursor ----------
        case WM_SETCURSOR: {
            static int lastHit = -1;
            int hit = LOWORD(lParam);
#if defined(DEBUG_INPUT_OS)
            if (hit != lastHit) {
                safe_log("[WndProc] WM_SETCURSOR hit test: " + std::to_string(hit) + "\n");
                lastHit = hit;
            }
#endif
            if (hit == HTCLIENT) {
                return FALSE; // Let Windows set default cursor
            }
            else {
                return DefWindowProc(hwnd, msg, wParam, lParam);
            }
        }

                         // ---------- Hit Test ----------
        case WM_NCHITTEST: {
#if defined(DEBUG_INPUT_OS)
            safe_log("[WndProc] WM_NCHITTEST\n");
#endif
            POINT pt = { LOWORD(lParam), HIWORD(lParam) };
            ScreenToClient(hwnd, &pt);

            RECT rc; GetClientRect(hwnd, &rc);
            int width = rc.right - rc.left;
            int height = rc.bottom - rc.top;

            constexpr int border = 8;
            RECT titleBar = { 0, 0, width, 30 };

            if (PtInRect(&titleBar, pt)) return HTCAPTION;

            if (pt.x < border) {
                if (pt.y < border) return HTTOPLEFT;
                if (pt.y > height - border) return HTBOTTOMLEFT;
                return HTLEFT;
            }
            if (pt.x > width - border) {
                if (pt.y < border) return HTTOPRIGHT;
                if (pt.y > height - border) return HTBOTTOMRIGHT;
                return HTRIGHT;
            }
            if (pt.y < border) return HTTOP;
            if (pt.y > height - border) return HTBOTTOM;

            return HTCLIENT;
        }

                         // ---------- Sizing ----------
        case WM_GETMINMAXINFO:
#if defined(DEBUG_INPUT_OS)
            safe_log("[WndProc] WM_GETMINMAXINFO (127) Window sizing query - Normal\n");
#endif
            break;

            // ---------- Misc ----------
        case WM_ACTIVATE:
#if defined(DEBUG_INPUT_OS)
            safe_log("[WndProc] WM_ACTIVATE (6) Window activation - Normal\n");
#endif
            break;

        case WM_SYNCPAINT:
#if defined(DEBUG_INPUT_OS)
            safe_log("[WndProc] WM_SYNCPAINT (28) Sync paint - Normal\n");
#endif
            break;

        case WM_KILLFOCUS:
#if defined(DEBUG_INPUT_OS)
            safe_log("[WndProc] WM_KILLFOCUS (8) Lost keyboard focus - Normal\n");
#endif
            break;

        case WM_DPICHANGED:
#if defined(DEBUG_INPUT_OS)
            safe_log("[WndProc] WM_DPICHANGED (641) DPI change - Normal\n");
#endif
            break;

        case WM_DPICHANGED_BEFOREPARENT:
#if defined(DEBUG_INPUT_OS)
            safe_log("[WndProc] WM_DPICHANGED_BEFOREPARENT (642) DPI change before parent - Normal\n");
#endif
            break;

            // ---------- Shutdown ----------
        case WM_CLOSE:
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
#if defined(DEBUG_INPUT_OS)
            safe_log_number(msg);
#endif
            break;
        }
#ifdef ALMOND_USING_OPENGL
        if (!s_openglstate.getOldWndProc())
            safe_log("[WndProc] WARNING: oldWndProc is NULL! Falling back to DefWindowProc.\n");

        return s_openglstate.getOldWndProc() && IsWindow(hwnd)
            ? CallWindowProc(s_openglstate.getOldWndProc(), hwnd, msg, wParam, lParam)
            : DefWindowProc(hwnd, msg, wParam, lParam);
#endif // !ALMOND_USING_OPENGL
    }
    catch (const std::exception& e) {
        safe_log(std::string("[WndProc] EXCEPTION: ") + e.what() + "\n");
		return DefWindowProc(hwnd, msg, wParam, lParam);

    }
    catch (...) {
        safe_log("[WndProc] EXCEPTION! Fallback to DefWindowProc.\n");
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}
#endif // !ALMOND_USING_RAYLIB
#endif // !ALMOND_MAIN_HEADLESS
#endif
