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
 // acontextcontrol.hpp
#pragma once

#include "aplatform.hpp"          // must be first
#include "aengineconfig.hpp"     // engine configuration
#include "acontexttype.hpp"

#include <atomic>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <variant>
#include <utility>

#ifndef _WIN32
using HWND = void*; // placeholder if you later add non-Win32 platforms
#endif

namespace almondnamespace::core { struct Context; }

namespace almondnamespace::user 
{
    // --------- Lock-free SPSC ring (single-producer / single-consumer) ---------
    template <typename T, std::size_t CapacityPow2 = 1024>
    class SPSCQueue 
    {
        static_assert((CapacityPow2& (CapacityPow2 - 1)) == 0, "Capacity must be power of two");
        alignas(64) std::array<T, CapacityPow2> buf_;
        alignas(64) std::atomic<uint32_t> head_{ 0 }; // consumer
        alignas(64) std::atomic<uint32_t> tail_{ 0 }; // producer
    public:
        bool try_push(const T& v) noexcept 
        {
            const uint32_t t = tail_.load(std::memory_order_relaxed);
            const uint32_t h = head_.load(std::memory_order_acquire);
            if (((t + 1) & (CapacityPow2 - 1)) == (h & (CapacityPow2 - 1))) return false; // full
            buf_[t & (CapacityPow2 - 1)] = v;
            tail_.store((t + 1) & (CapacityPow2 - 1), std::memory_order_release);
            return true;
        }
        bool try_push(T&& v) noexcept 
        {
            const uint32_t t = tail_.load(std::memory_order_relaxed);
            const uint32_t h = head_.load(std::memory_order_acquire);
            if (((t + 1) & (CapacityPow2 - 1)) == (h & (CapacityPow2 - 1))) return false;
            buf_[t & (CapacityPow2 - 1)] = std::move(v);
            tail_.store((t + 1) & (CapacityPow2 - 1), std::memory_order_release);
            return true;
        }
        bool try_pop(T& out) noexcept 
        {
            const uint32_t h = head_.load(std::memory_order_relaxed);
            const uint32_t t = tail_.load(std::memory_order_acquire);
            if (h == t) return false; // empty
            out = std::move(buf_[h & (CapacityPow2 - 1)]);
            head_.store((h + 1) & (CapacityPow2 - 1), std::memory_order_release);
            return true;
        }
        template <typename F>
        uint32_t drain(F&& fn, uint32_t max = UINT32_MAX) noexcept 
        {
            uint32_t n = 0; T item;
            while (n < max && try_pop(item)) { fn(std::move(item)); ++n; }
            return n;
        }
    };

    // --------- Command definitions ----------
    enum class Op : uint16_t 
    {
        PresentNow,
        ClearNow,
        Resize,             // payload: width, height
        ToggleVSync,        // payload: bool on
        SetActive,          // payload: bool active
        EnsureAtlasUploaded,// payload: atlasIndex
        DrawSprite,         // payload: (atlasIndex, localIndex, x,y,w,h)
        RunScriptOnce,      // payload: (string/id) – you can fill later
        CustomCallable,     // payload: function<void(core::Context&)>
        Quiesce,            // render thread should stop issuing GPU calls & mark paused
        Resume,             // resume rendering
        Shutdown
    };

    struct ResizePayload { int w{}, h{}; };
    struct TogglePayload { bool on{}; };
    struct ActivePayload { bool active{}; };
    struct AtlasPayload { int atlasIndex{}; };
    struct SpriteHandlePayload {
        uint32_t atlasIndex{}, localIndex{};
        float x{}, y{}, w{}, h{};
    };

    struct Callable { using Fn = std::function<void(almondnamespace::core::Context&)>; Fn fn; };

    using Payload = std::variant<std::monostate,
        ResizePayload,
        TogglePayload,
        ActivePayload,
        AtlasPayload,
        SpriteHandlePayload,
        Callable>;

    struct Command { Op op{}; Payload payload{}; };

    // --------- Per-window mailbox (render thread consumes) ----------
    struct Mailbox 
    {
        SPSCQueue<Command, 1024> q;
        bool post(Command c) noexcept { return q.try_push(std::move(c)); }
        template <typename F>
        uint32_t drain(F&& fn, uint32_t max = UINT32_MAX) noexcept {
            return q.drain(std::forward<F>(fn), max);
        }
    };

    // --------- Global broker (no locks; tiny fixed array) ----------
    class ControlBus 
    {
    public:
        static ControlBus& instance() { static ControlBus bus; return bus; }

        void register_mailbox(HWND hwnd, Mailbox* box) noexcept {
            for (auto& e : entries_) if (e.hwnd == hwnd || e.hwnd == nullptr) { e.hwnd = hwnd; e.box = box; return; }
        }
        void unregister_mailbox(HWND hwnd) noexcept {
            for (auto& e : entries_) if (e.hwnd == hwnd) { e.hwnd = nullptr; e.box = nullptr; return; }
        }

        bool post(HWND hwnd, Command c) noexcept {
            if (!hwnd) return false;
            if (auto* mb = find(hwnd)) return mb->post(std::move(c));
            return false;
        }
        uint32_t broadcast(const Command& c) noexcept {
            uint32_t ok = 0;
            for (auto& e : entries_) if (e.box && e.hwnd) ok += e.box->post(c) ? 1u : 0u;
            return ok;
        }

    private:
        struct Entry { HWND hwnd{}; Mailbox* box{}; };
        std::array<Entry, 32> entries_{}; // grow if needed
        Mailbox* find(HWND hwnd) noexcept { for (auto& e : entries_) if (e.hwnd == hwnd) return e.box; return nullptr; }
    };

    // --------- Convenience builders ----------
    inline Command present_now() { return { Op::PresentNow, {} }; }
    inline Command clear_now() { return { Op::ClearNow,   {} }; }
    inline Command resize(int w, int h) { return { Op::Resize, ResizePayload{w,h} }; }
    inline Command vsync(bool on) { return { Op::ToggleVSync, TogglePayload{on} }; }
    inline Command set_active(bool a) { return { Op::SetActive, ActivePayload{a} }; }
    inline Command ensure_atlas_uploaded(int idx) { return { Op::EnsureAtlasUploaded, AtlasPayload{idx} }; }
    inline Command draw_sprite_cmd(uint32_t aIdx, uint32_t lIdx, float x, float y, float w, float h) {
        return { Op::DrawSprite, SpriteHandlePayload{aIdx,lIdx,x,y,w,h} };
    }
    inline Command call_on_context(Callable::Fn fn) { return { Op::CustomCallable, Callable{std::move(fn)} }; }

} // namespace almondnamespace::control
