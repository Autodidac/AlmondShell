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
 // agui.cpp
#include "pch.h"

#include "agui.hpp"

#include "aatlasmanager.hpp"
#include "acontext.hpp"
#include "awindowdata.hpp"
#include "aspritepool.hpp"
#include "aspriteregistry.hpp"
#include "atexture.hpp"
#include "afontrenderer.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace almondnamespace::gui
{
namespace
{
    using Vec2 = almondnamespace::gui::Vec2;
    using Color = ::almondnamespace::gui::Color;
    using EventType = almondnamespace::gui::EventType;
    using InputEvent = ::almondnamespace::gui::InputEvent;

    constexpr const char* kAtlasName = "__agui_builtin";
    constexpr float kContentPadding = 8.0f;
    constexpr float kDefaultFontSizePt = 18.0f;
    constexpr float kFontScale = 1.0f;
    constexpr float kTitleScale = 1.4f;
    constexpr float kBoxInnerPadding = 6.0f;
    constexpr float kCaretBlinkPeriod = 1.0f;
    constexpr int kTabSpaces = 4;
    constexpr const char* kDefaultFontName = "__agui_default_font";
    constexpr const char* kDefaultFontFile = "Roboto-Regular.ttf";

    struct GuiFontCache
    {
        std::string fontName = kDefaultFontName;
        float fontSizePt = kDefaultFontSizePt;
        const font::FontAsset* asset = nullptr;
        const TextureAtlas* atlas = nullptr;
        font::FontMetrics metrics{};
        std::array<const font::Glyph*, 256> glyphLookup{};
        const font::Glyph* fallbackGlyph = nullptr;
    };

    struct GuiResources
    {
        bool atlasBuilt = false;
        TextureAtlas* atlas = nullptr;
        SpriteHandle windowBackground{};
        SpriteHandle buttonNormal{};
        SpriteHandle buttonHover{};
        SpriteHandle buttonActive{};
        SpriteHandle textField{};
        SpriteHandle textFieldActive{};
        SpriteHandle panelBackground{};
        SpriteHandle consoleBackground{};
        GuiFontCache font{};
        font::FontRenderer fontRenderer{};
    };

    GuiResources g_resources{};
    std::mutex g_resourceMutex;

    struct PtrHash
    {
        size_t operator()(const void* p) const noexcept
        {
            return std::hash<const void*>{}(p);
        }
    };

    struct UploadState
    {
        bool guiAtlasUploaded = false;
        bool fontAtlasUploaded = false;
    };

    static std::unordered_map<const void*, UploadState, PtrHash> g_uploadedContexts;
    static std::mutex g_uploadMutex;

    [[nodiscard]] std::filesystem::path find_default_font_path()
    {
        auto resolve_env_font_path = []() -> std::filesystem::path
        {
#if defined(_WIN32)
            char* envBuf = nullptr;
            size_t len = 0;

            if (_dupenv_s(&envBuf, &len, "ALMOND_GUI_FONT_PATH") == 0 && envBuf)
            {
                std::filesystem::path envPath{ envBuf };
                free(envBuf);
                return envPath;
            }
            return {};
#else
            if (const char* envValue = std::getenv("ALMOND_GUI_FONT_PATH"))
                return std::filesystem::path{ envValue };
            return {};
#endif
        };

        if (std::filesystem::path envPath = resolve_env_font_path(); !envPath.empty())
        {
            std::error_code ec;
            if (std::filesystem::exists(envPath, ec))
                return envPath;
        }

        const std::array<std::filesystem::path, 5> relativeCandidates{
            std::filesystem::path{kDefaultFontFile},
            std::filesystem::path{"assets/fonts"} / kDefaultFontFile,
            std::filesystem::path{"Fonts"} / kDefaultFontFile,
            std::filesystem::path{"AlmondShell/assets/fonts"} / kDefaultFontFile,
            std::filesystem::path{"../AlmondShell/assets/fonts"} / kDefaultFontFile,
        };

        const auto try_with_root = [&](const std::filesystem::path& root) -> std::filesystem::path {
            for (const auto& rel : relativeCandidates)
            {
                std::filesystem::path candidate = root.empty() ? rel : (root / rel);
                std::error_code ec;
                if (!candidate.empty() && std::filesystem::exists(candidate, ec))
                    return candidate;
            }
            return {};
            };

        if (auto path = try_with_root({}); !path.empty())
            return path;

        const std::filesystem::path cwd = std::filesystem::current_path();
        if (auto path = try_with_root(cwd); !path.empty())
            return path;

        const std::filesystem::path parent = cwd.parent_path();
        if (!parent.empty())
        {
            if (auto path = try_with_root(parent); !path.empty())
                return path;
        }

        return {};
    }

    void populate_font_lookup(GuiFontCache& font)
    {
        font.glyphLookup.fill(nullptr);
        font.fallbackGlyph = nullptr;

        if (!font.asset)
            return;

        for (const auto& [codepoint, glyph] : font.asset->glyphs)
        {
            if (codepoint < static_cast<char32_t>(font.glyphLookup.size()))
            {
                font.glyphLookup[static_cast<std::size_t>(codepoint)] = &glyph;
            }
        }

        auto setFallback = [&](unsigned char ch)
        {
            if (ch < font.glyphLookup.size() && font.glyphLookup[ch])
                font.fallbackGlyph = font.glyphLookup[ch];
        };

        setFallback(static_cast<unsigned char>('?'));
        if (!font.fallbackGlyph)
            setFallback(static_cast<unsigned char>(' '));
        if (!font.fallbackGlyph && !font.asset->glyphs.empty())
            font.fallbackGlyph = &font.asset->glyphs.begin()->second;
    }

    void ensure_font_loaded_locked()
    {
        if (g_resources.font.asset)
            return;

        const std::filesystem::path fontPath = find_default_font_path();
        if (fontPath.empty())
        {
            std::cerr << "[agui] Unable to locate GUI font '" << kDefaultFontFile << "'\n";
            return;
        }

        if (!g_resources.fontRenderer.load_font(g_resources.font.fontName, fontPath.string(), g_resources.font.fontSizePt))
        {
            std::cerr << "[agui] Failed to load GUI font from '" << fontPath.string() << "'\n";
            return;
        }

        g_resources.font.asset = g_resources.fontRenderer.get_font(g_resources.font.fontName);
        if (!g_resources.font.asset)
        {
            std::cerr << "[agui] Font renderer returned no asset for '" << g_resources.font.fontName << "'\n";
            return;
        }

        g_resources.font.metrics = g_resources.font.asset->metrics;
        populate_font_lookup(g_resources.font);

        const auto& atlasVec = almondnamespace::atlasmanager::get_atlas_vector();
        if (g_resources.font.asset->atlas_index >= 0 &&
            static_cast<std::size_t>(g_resources.font.asset->atlas_index) < atlasVec.size())
        {
            g_resources.font.atlas = atlasVec[g_resources.font.asset->atlas_index];
            if (g_resources.font.atlas)
            {
                almondnamespace::atlasmanager::ensure_uploaded(*g_resources.font.atlas);
            }
        }
    }

    struct FrameState {
        // thread's active render context (must be set in begin_frame)
        std::shared_ptr<core::Context> ctxShared{};
        core::Context* ctx = nullptr;

        Vec2 cursor{};
        Vec2 origin{};
        Vec2 windowSize{};
        Vec2 mousePos{};

        bool mouseDown = false;
        bool prevMouseDown = false;
        bool justReleased = false;
        bool insideWindow = false;
        bool justPressed = false;

        std::optional<WidgetBounds> lastButtonBounds{};

        float deltaTime = 0.0f;
        float caretTimer = 0.0f;
        bool caretVisible = true;

        std::vector<InputEvent> events{};
    };

    thread_local FrameState g_frame{};
    thread_local std::vector<InputEvent> g_pendingEvents{};
    thread_local const void* g_activeWidget = nullptr;

    // Cheap tripwire so this doesn't regress again:
    static_assert(std::is_same_v<decltype(g_frame.ctx), core::Context*>,
        "FrameState must carry core::Context* ctx");

    //constexpr const char* kAtlasName = "__agui_builtin";
    //constexpr float kContentPadding = 8.0f;
    //constexpr float kFontScale = 1.0f;
    //constexpr float kTitleScale = 1.4f;
    //constexpr float kBoxInnerPadding = 6.0f;
    //constexpr float kCaretBlinkPeriod = 1.0f;

    [[nodiscard]] std::vector<std::uint8_t> make_solid_pixels(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a, std::uint32_t w, std::uint32_t h)
    {
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w) * h * 4, 0);
        for (std::size_t idx = 0; idx < pixels.size(); idx += 4) {
            pixels[idx + 0] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = a;
        }
        return pixels;
    }

    [[nodiscard]] SpriteHandle add_sprite(TextureAtlas& atlas, const std::string& name, const std::vector<std::uint8_t>& pixels, std::uint32_t w, std::uint32_t h)
    {
        Texture texture{};
        texture.name = name;
        texture.width = w;
        texture.height = h;
        texture.channels = 4;
        texture.pixels = pixels;

        auto entry = atlas.add_entry(name, texture);
        if (!entry) {
            throw std::runtime_error("[agui] Failed to add atlas entry: " + name);
        }

        if (almondnamespace::spritepool::capacity == 0) {
            almondnamespace::spritepool::initialize(2048);
        }

        SpriteHandle handle = almondnamespace::spritepool::allocate();
        if (!handle.is_valid()) {
            throw std::runtime_error("[agui] Sprite pool exhausted while registering GUI sprite");
        }

        handle.atlasIndex = static_cast<std::uint32_t>(atlas.get_index());
        handle.localIndex = static_cast<std::uint32_t>(entry->index);

        almondnamespace::atlasmanager::registry.add(name, handle,
            entry->region.u1,
            entry->region.v1,
            entry->region.u2 - entry->region.u1,
            entry->region.v2 - entry->region.v1);

        return handle;
    }

    void ensure_resources()
    {
        std::scoped_lock lock(g_resourceMutex);
        if (!g_resources.atlasBuilt)
        {
            auto atlasIt = almondnamespace::atlasmanager::atlas_map.find(kAtlasName);
            if (atlasIt == almondnamespace::atlasmanager::atlas_map.end()) {
                almondnamespace::atlasmanager::create_atlas({
                    .name = kAtlasName,
                    .width = 512,
                    .height = 512,
                    .generate_mipmaps = false
                });
                atlasIt = almondnamespace::atlasmanager::atlas_map.find(kAtlasName);
                if (atlasIt == almondnamespace::atlasmanager::atlas_map.end()) {
                    throw std::runtime_error("[agui] Unable to create GUI atlas");
                }
            }

            TextureAtlas& atlas = atlasIt->second;
            g_resources.atlas = &atlas;

            g_resources.windowBackground = add_sprite(atlas, "__agui/window_bg",
                make_solid_pixels(0x33, 0x35, 0x38, 0xFF, 8, 8), 8, 8);
            g_resources.buttonNormal = add_sprite(atlas, "__agui/button_normal",
                make_solid_pixels(0x5B, 0x5F, 0x66, 0xFF, 8, 8), 8, 8);
            g_resources.buttonHover = add_sprite(atlas, "__agui/button_hover",
                make_solid_pixels(0x76, 0x7C, 0x85, 0xFF, 8, 8), 8, 8);
            g_resources.buttonActive = add_sprite(atlas, "__agui/button_active",
                make_solid_pixels(0x94, 0x9A, 0xA3, 0xFF, 8, 8), 8, 8);
            g_resources.textField = add_sprite(atlas, "__agui/text_field",
                make_solid_pixels(0x2B, 0x2E, 0x33, 0xFF, 8, 8), 8, 8);
            g_resources.textFieldActive = add_sprite(atlas, "__agui/text_field_active",
                make_solid_pixels(0x3A, 0x3E, 0x45, 0xFF, 8, 8), 8, 8);
            g_resources.panelBackground = add_sprite(atlas, "__agui/panel_bg",
                make_solid_pixels(0x27, 0x29, 0x2E, 0xFF, 8, 8), 8, 8);
            g_resources.consoleBackground = add_sprite(atlas, "__agui/console_bg",
                make_solid_pixels(0x1F, 0x21, 0x26, 0xFF, 8, 8), 8, 8);

            g_resources.atlasBuilt = true;
        }

        ensure_font_loaded_locked();
    }

    void ensure_backend_upload(core::Context& ctx)
    {
        ensure_resources();
        std::scoped_lock lock(g_uploadMutex);
        auto& state = g_uploadedContexts[&ctx];

        if (!state.guiAtlasUploaded && g_resources.atlas)
        {
            const std::uint32_t handle = ctx.add_atlas_safe(*g_resources.atlas);
            if (handle == 0) {
                throw std::runtime_error("[agui] Failed to upload GUI atlas for this context");
            }
            state.guiAtlasUploaded = true;
        }

        if (!state.fontAtlasUploaded && g_resources.font.atlas)
        {
            const std::uint32_t handle = ctx.add_atlas_safe(*g_resources.font.atlas);
            if (handle != 0)
            {
                state.fontAtlasUploaded = true;
            }
        }
    }

    void draw_sprite(const SpriteHandle& handle, float x, float y, float w, float h)
    {
        if (!handle.is_valid())
            return;

        core::Context* ctx = g_frame.ctx;
        if (!ctx)
            return;

        bool queued = false;
        if (ctx->windowData && g_frame.ctxShared) {
            auto ctxShared = g_frame.ctxShared;
            ctx->windowData->commandQueue.enqueue([ctxShared,
                handle,
                x,
                y,
                w,
                h]() {
                    if (!ctxShared)
                        return;
                    const auto& atlasesRT = almondnamespace::atlasmanager::get_atlas_vector();
                    ctxShared->draw_sprite_safe(handle, atlasesRT, x, y, w, h);
                });
            queued = true;
        }

        if (!queued) {
            const auto& atlases = almondnamespace::atlasmanager::get_atlas_vector();
            ctx->draw_sprite_safe(handle, atlases, x, y, w, h);
        }
    }

    [[nodiscard]] bool point_in_rect(Vec2 point, float x, float y, float w, float h) noexcept
    {
        return (point.x >= x && point.x <= x + w && point.y >= y && point.y <= y + h);
    }

    [[nodiscard]] float base_line_height(float scale) noexcept
    {
        const auto& metrics = g_resources.font.metrics;
        if (metrics.ascent > 0.0f || metrics.descent > 0.0f)
            return (metrics.ascent + metrics.descent) * scale;
        return 8.0f * scale;
    }

    [[nodiscard]] float line_advance_amount(float scale) noexcept
    {
        const auto& metrics = g_resources.font.metrics;
        if (metrics.ascent > 0.0f || metrics.descent > 0.0f || metrics.lineGap > 0.0f)
            return (metrics.ascent + metrics.descent + metrics.lineGap) * scale;
        return base_line_height(scale);
    }

    [[nodiscard]] float baseline_offset(float scale) noexcept
    {
        const auto& metrics = g_resources.font.metrics;
        if (metrics.ascent > 0.0f)
            return metrics.ascent * scale;
        return base_line_height(scale);
    }

    [[nodiscard]] float space_advance(float scale) noexcept
    {
        if (g_resources.font.metrics.spaceAdvance > 0.0f)
            return g_resources.font.metrics.spaceAdvance * scale;
        if (const auto* glyph = g_resources.font.glyphLookup[static_cast<unsigned char>(' ')])
            return glyph->advance * scale;
        if (g_resources.font.metrics.averageAdvance > 0.0f)
            return g_resources.font.metrics.averageAdvance * scale;
        return 8.0f * scale;
    }

    [[nodiscard]] const font::Glyph* lookup_glyph(unsigned char ch) noexcept
    {
        if (!g_resources.font.asset)
            return nullptr;

        if (ch < g_resources.font.glyphLookup.size())
        {
            if (const auto* glyph = g_resources.font.glyphLookup[ch])
                return glyph;
        }

        return g_resources.font.fallbackGlyph;
    }

    [[nodiscard]] std::optional<unsigned char> next_drawable_char(std::string_view text, std::size_t index) noexcept
    {
        if (index >= text.size())
            return std::nullopt;

        for (std::size_t i = index + 1; i < text.size(); ++i)
        {
            const char next = text[i];
            if (next == '\n')
                return std::nullopt;
            return static_cast<unsigned char>(next);
        }

        return std::nullopt;
    }

    [[nodiscard]] float kerning_adjust(unsigned char left, unsigned char right, float scale) noexcept
    {
        if (!g_resources.font.asset)
            return 0.0f;

        const float kern = g_resources.font.asset->get_kerning(left, right);
        if (kern == 0.0f)
            return 0.0f;
        return kern * scale;
    }

    [[nodiscard]] float glyph_advance(unsigned char ch, float scale) noexcept
    {
        if (ch == '\t')
            return space_advance(scale) * static_cast<float>(kTabSpaces);

        if (const auto* glyph = lookup_glyph(ch))
            return glyph->advance * scale;

        if (g_resources.font.metrics.averageAdvance > 0.0f)
            return g_resources.font.metrics.averageAdvance * scale;

        return 8.0f * scale;
    }

    [[nodiscard]] float glyph_advance_with_kerning(unsigned char ch,
        std::optional<unsigned char> next,
        float scale) noexcept
    {
        float advance = glyph_advance(ch, scale);
        if (next)
            advance += kerning_adjust(ch, *next, scale);
        return advance;
    }

    [[nodiscard]] float measure_text_width(std::string_view text, float scale) noexcept
    {
        float current = 0.0f;
        float maxWidth = 0.0f;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const char ch = text[i];
            if (ch == '\n')
            {
                maxWidth = (std::max)(maxWidth, current);
                current = 0.0f;
                continue;
            }
            const auto next = next_drawable_char(text, i);
            current += glyph_advance_with_kerning(static_cast<unsigned char>(ch), next, scale);
        }
        return (std::max)(maxWidth, current);
    }

    [[nodiscard]] float measure_wrapped_text_height(std::string_view text, float width, float scale) noexcept
    {
        ensure_resources();
        const float effectiveWidth = (std::max)(space_advance(scale), width);
        const float lineAdvance = line_advance_amount(scale);
        const float baseHeight = base_line_height(scale);

        std::size_t lines = 1;
        float penX = 0.0f;
        for (std::size_t i = 0; i < text.size(); ++i) {
            const char ch = text[i];
            if (ch == '\n') {
                ++lines;
                penX = 0.0f;
                continue;
            }

            const auto next = next_drawable_char(text, i);
            const float advance = glyph_advance_with_kerning(static_cast<unsigned char>(ch), next, scale);
            if (penX + advance > effectiveWidth + 0.001f) {
                ++lines;
                penX = 0.0f;
            }

            penX += advance;
        }

        const float totalHeight = baseHeight + static_cast<float>(lines - 1) * lineAdvance;
        return (std::max)(baseHeight, totalHeight);
    }

    [[nodiscard]] almondnamespace::gui::Vec2 compute_caret_position(std::string_view text, float x, float y, float width, float scale) noexcept
    {
        ensure_resources();
        const float effectiveWidth = (std::max)(space_advance(scale), width);
        const float lineAdvance = line_advance_amount(scale);
        float penX = x;
        float baseline = y + baseline_offset(scale);

        for (std::size_t i = 0; i < text.size(); ++i) {
            const char ch = text[i];
            if (ch == '\n') {
                penX = x;
                baseline += lineAdvance;
                continue;
            }

            const auto next = next_drawable_char(text, i);
            const float advance = glyph_advance_with_kerning(static_cast<unsigned char>(ch), next, scale);
            if (penX + advance > x + effectiveWidth + 0.001f) {
                penX = x;
                baseline += lineAdvance;
            }

            penX += advance;
        }

        const float caretTop = baseline - baseline_offset(scale);
        return { penX, caretTop };
    }

    float draw_wrapped_text(std::string_view text, float x, float y, float width, float scale)
    {
        ensure_resources();
        if (!g_frame.ctx || !g_resources.font.asset)
            return 0.0f;

        const float effectiveWidth = (std::max)(space_advance(scale), width);
        const float lineAdvance = line_advance_amount(scale);
        const float ascent = baseline_offset(scale);
        const float baseHeight = base_line_height(scale);

        float penX = x;
        float baseline = y + ascent;
        std::size_t lines = 1;

        for (std::size_t i = 0; i < text.size(); ++i) {
            const char ch = text[i];
            if (ch == '\n') {
                penX = x;
                baseline += lineAdvance;
                ++lines;
                continue;
            }

            const auto next = next_drawable_char(text, i);
            const float advance = glyph_advance_with_kerning(static_cast<unsigned char>(ch), next, scale);
            if (penX + advance > x + effectiveWidth + 0.001f) {
                penX = x;
                baseline += lineAdvance;
                ++lines;
            }

            if (ch != ' ' && ch != '\t') {
                if (const auto* glyph = lookup_glyph(static_cast<unsigned char>(ch))) {
                    const float drawW = glyph->size_px.x * scale;
                    const float drawH = glyph->size_px.y * scale;
                    if (glyph->handle.is_valid() && drawW > 0.0f && drawH > 0.0f) {
                        const float offsetX = glyph->offset_px.x * scale;
                        const float offsetY = glyph->offset_px.y * scale;
                        draw_sprite(glyph->handle, penX + offsetX, baseline + offsetY, drawW, drawH);
                    }
                }
            }

            penX += advance;
        }

        return baseHeight + static_cast<float>(lines - 1) * lineAdvance;
    }

    void draw_caret(float x, float y, float height)
    {
        const float caretWidth = (std::max)(1.0f, space_advance(kFontScale) * 0.1f);
        draw_sprite(g_resources.buttonActive, x, y, caretWidth, height);
    }

    void draw_text_line(std::string_view text, float x, float y, float scale, std::optional<float> indent = std::nullopt)
    {
        ensure_resources();
        if (!g_frame.ctx || !g_resources.font.asset)
            return;

        const float anchorX = indent.value_or(x);
        float penX = anchorX;
        float baseline = y + baseline_offset(scale);
        const float lineAdvance = line_advance_amount(scale);

        for (std::size_t i = 0; i < text.size(); ++i) {
            const char ch = text[i];
            if (ch == '\n') {
                penX = anchorX;
                baseline += lineAdvance;
                continue;
            }

            if (const auto* glyph = lookup_glyph(static_cast<unsigned char>(ch))) {
                if (glyph->handle.is_valid()) {
                    const float drawW = glyph->size_px.x * scale;
                    const float drawH = glyph->size_px.y * scale;
                    const float offsetX = glyph->offset_px.x * scale;
                    const float offsetY = glyph->offset_px.y * scale;
                    draw_sprite(glyph->handle, penX + offsetX, baseline + offsetY, drawW, drawH);
                }
            }

            const auto next = next_drawable_char(text, i);
            penX += glyph_advance_with_kerning(static_cast<unsigned char>(ch), next, scale);
        }
    }

    void reset_frame()
    {
        g_frame.cursor = {};
        g_frame.origin = {};
        g_frame.windowSize = {};
        g_frame.insideWindow = false;
        g_frame.lastButtonBounds.reset();
    }


} // namespace

void set_cursor(Vec2 position) noexcept
{
    if (!g_frame.insideWindow)
        return;

    g_frame.cursor = position;
}

void advance_cursor(Vec2 delta) noexcept
{
    if (!g_frame.insideWindow)
        return;

    g_frame.cursor += delta;
}

void push_input(const almondnamespace::gui::InputEvent& e) noexcept
{
    g_pendingEvents.push_back(e);
}

void begin_frame(const std::shared_ptr<core::Context>& ctx, float dt, Vec2 mouse_pos, bool mouse_down) noexcept
{
    core::Context* rawCtx = ctx.get();
    if (rawCtx) {
        try {
            ensure_backend_upload(*rawCtx);
        }
        catch (...) {
            // swallow: GUI is optional, avoid propagating exceptions across frames
        }
    }

    g_frame.ctxShared = ctx;
    g_frame.ctx = rawCtx;
    g_frame.deltaTime = dt;

    g_frame.caretTimer += dt;
    while (g_frame.caretTimer >= kCaretBlinkPeriod) {
        g_frame.caretTimer -= kCaretBlinkPeriod;
    }
    g_frame.caretVisible = (g_frame.caretTimer < (kCaretBlinkPeriod * 0.5f));

    const bool prevMouseDown = g_frame.mouseDown;
    bool currentMouseDown = mouse_down;
    Vec2 currentMousePos = mouse_pos;

    g_frame.events.clear();
    if (!g_pendingEvents.empty()) {
        g_frame.events.insert(g_frame.events.end(), g_pendingEvents.begin(), g_pendingEvents.end());
        g_pendingEvents.clear();
    }

    for (const auto& evt : g_frame.events) {
        switch (evt.type) {
        case EventType::MouseMove:
            currentMousePos = evt.mouse_pos;
            break;
        case EventType::MouseDown:
            currentMouseDown = true;
            currentMousePos = evt.mouse_pos;
            break;
        case EventType::MouseUp:
            currentMouseDown = false;
            currentMousePos = evt.mouse_pos;
            break;
        default:
            break;
        }
    }

    g_frame.prevMouseDown = prevMouseDown;
    g_frame.mouseDown = currentMouseDown;
    g_frame.mousePos = currentMousePos;
    g_frame.justPressed = (!prevMouseDown && currentMouseDown);
    g_frame.justReleased = (prevMouseDown && !currentMouseDown);
    reset_frame();
}

void end_frame() noexcept
{
    g_frame.ctxShared.reset();
    g_frame.ctx = nullptr;
    g_frame.insideWindow = false;
    g_frame.events.clear();
    g_frame.justPressed = false;
    g_frame.justReleased = false;
}

void begin_window(std::string_view title, Vec2 position, Vec2 size) noexcept
{
    if (!g_frame.ctx)
        return;

    ensure_resources();

    g_frame.origin = position;
    g_frame.windowSize = size;
    g_frame.insideWindow = true;
    set_cursor({ position.x + kContentPadding, position.y + kContentPadding });

    draw_sprite(g_resources.windowBackground, position.x, position.y, size.x, size.y);

    const float titleHeight = base_line_height(kTitleScale);
    draw_text_line(title, position.x + kContentPadding, position.y + kContentPadding, kTitleScale);

    advance_cursor({ 0.0f, titleHeight + kContentPadding });
}

void end_window() noexcept
{
    g_frame.insideWindow = false;
}

bool button(std::string_view label, Vec2 size) noexcept
{
    if (!g_frame.insideWindow || !g_frame.ctx)
        return false;

    const Vec2 pos = g_frame.cursor;
    const float baseHeight = base_line_height(kFontScale);
    const float minWidth = space_advance(kFontScale) + 2.0f * kContentPadding;
    const float width = (std::max)(static_cast<float>(size.x), minWidth);
    const float height = (std::max)(static_cast<float>(size.y), baseHeight + 2.0f * kContentPadding);

    const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height);

    const SpriteHandle background = hovered
        ? (g_frame.mouseDown ? g_resources.buttonActive : g_resources.buttonHover)
        : g_resources.buttonNormal;

    draw_sprite(background, pos.x, pos.y, width, height);

    const float textWidth = measure_text_width(label, kFontScale);
    const float textHeight = baseHeight;
    const float textX = pos.x + (std::max)(0.0f, (width - textWidth) * 0.5f);
    const float textY = pos.y + (std::max)(0.0f, (height - textHeight) * 0.5f);
    draw_text_line(label, textX, textY, kFontScale);

    g_frame.lastButtonBounds = WidgetBounds{ pos, { width, height } };
    advance_cursor({ 0.0f, height + kContentPadding });
    return hovered && g_frame.justPressed;
}

bool image_button(const SpriteHandle& sprite, Vec2 size) noexcept
{
    if (!g_frame.insideWindow || !g_frame.ctx)
        return false;

    ensure_resources();

    const Vec2 pos = g_frame.cursor;
    const float minSize = base_line_height(kFontScale) * 2.0f;
    const float width = (std::max)(static_cast<float>(size.x), minSize);
    const float height = (std::max)(static_cast<float>(size.y), minSize);

    const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height);
    const SpriteHandle background = hovered
        ? (g_frame.mouseDown ? g_resources.buttonActive : g_resources.buttonHover)
        : g_resources.buttonNormal;

    draw_sprite(background, pos.x, pos.y, width, height);
    if (sprite.is_valid()) {
        draw_sprite(sprite, pos.x, pos.y, width, height);
    }

    g_frame.lastButtonBounds = WidgetBounds{ pos, { width, height } };
    advance_cursor({ 0.0f, height + kContentPadding });
    return hovered && g_frame.justPressed;
}

EditBoxResult edit_box(std::string& text, Vec2 size, std::size_t max_chars, bool multiline) noexcept
{
    EditBoxResult result{};
    if (!g_frame.insideWindow || !g_frame.ctx)
        return result;

    ensure_resources();

    const Vec2 pos = g_frame.cursor;
    const float baseHeight = base_line_height(kFontScale);
    const float minWidth = space_advance(kFontScale) * 4.0f;
    const float minHeight = baseHeight + 2.0f * kBoxInnerPadding;
    const float width = (std::max)(static_cast<float>(size.x), minWidth);
    const float height = (std::max)(static_cast<float>(size.y), minHeight);

    const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height);
    const void* id = static_cast<const void*>(&text);

    if (g_frame.justPressed) {
        if (hovered) {
            g_activeWidget = id;
            g_frame.caretTimer = 0.0f;
            g_frame.caretVisible = true;
        }
        else if (g_activeWidget == id) {
            g_activeWidget = nullptr;
        }
    }

    if (g_frame.justReleased && !hovered && g_activeWidget == id) {
        g_activeWidget = nullptr;
    }

    const bool active = (g_activeWidget == id);
    result.active = active;

    const SpriteHandle background = active ? g_resources.textFieldActive : g_resources.textField;
    draw_sprite(background, pos.x, pos.y, width, height);

    const float contentWidth = (std::max)(1.0f, width - 2.0f * kBoxInnerPadding);
    const float contentHeight = (std::max)(1.0f, height - 2.0f * kBoxInnerPadding);
    const float textX = pos.x + kBoxInnerPadding;
    const float textY = pos.y + kBoxInnerPadding;

    if (multiline) {
        draw_wrapped_text(text, textX, textY, contentWidth, kFontScale);
    }
    else {
        draw_text_line(text, textX, textY, kFontScale);
    }

    const std::size_t limit = max_chars == 0 ? std::numeric_limits<std::size_t>::max() : max_chars;

    if (active) {
        for (const auto& evt : g_frame.events) {
            switch (evt.type) {
            case EventType::TextInput:
                for (char ch : evt.text) {
                    if (ch == '\r')
                        continue;
                    if (ch == '\n') {
                        if (multiline) {
                            if (text.size() < limit) {
                                text.push_back('\n');
                                result.changed = true;
                            }
                        }
                        else {
                            result.submitted = true;
                        }
                        continue;
                    }
                    if (static_cast<unsigned char>(ch) < 32)
                        continue;
                    if (text.size() < limit) {
                        text.push_back(ch);
                        result.changed = true;
                    }
                }
                break;
            case EventType::KeyDown:
                if (evt.key == 8 || evt.key == 127) {
                    if (!text.empty()) {
                        text.pop_back();
                        result.changed = true;
                    }
                }
                else if (evt.key == 27) {
                    g_activeWidget = nullptr;
                    result.active = false;
                }
                else if (evt.key == 13) {
                    if (multiline) {
                        if (text.size() < limit) {
                            text.push_back('\n');
                            result.changed = true;
                        }
                    }
                    else {
                        result.submitted = true;
                    }
                }
                break;
            default:
                break;
            }
        }
    }

    if (active && g_frame.caretVisible) {
        const Vec2 caret = multiline
            ? compute_caret_position(text, textX, textY, contentWidth, kFontScale)
            : Vec2{ textX + measure_text_width(text, kFontScale), textY };

        const float caretHeight = multiline
            ? (std::min)(contentHeight, baseHeight)
            : baseHeight;

        const float caretRight = textX + (std::max)(1.0f, contentWidth) - 1.0f;
        const float caretX = std::clamp(caret.x, textX, caretRight);
        const float caretY = std::clamp(caret.y, textY, textY + (std::max)(0.0f, contentHeight - caretHeight));
        draw_caret(caretX, caretY, caretHeight);
    }

    advance_cursor({ 0.0f, height + kContentPadding });
    return result;
}

void text_box(std::string_view text, Vec2 size) noexcept
{
    if (!g_frame.insideWindow || !g_frame.ctx)
        return;

    ensure_resources();

    const Vec2 pos = g_frame.cursor;
    const float baseHeight = base_line_height(kFontScale);
    const float minWidth = space_advance(kFontScale) * 4.0f;
    float width = static_cast<float>(size.x);
    if (width <= 0.0f) {
        const float estimated = measure_text_width(text, kFontScale) + 2.0f * kBoxInnerPadding;
        width = (std::max)(minWidth, estimated);
    }
    else {
        width = (std::max)(width, minWidth);
    }

    const float contentWidth = (std::max)(1.0f, width - 2.0f * kBoxInnerPadding);
    float height = static_cast<float>(size.y);
    if (height <= 0.0f) {
        const float textHeight = measure_wrapped_text_height(text, contentWidth, kFontScale);
        height = textHeight + 2.0f * kBoxInnerPadding;
    }
    else {
        height = (std::max)(height, baseHeight + 2.0f * kBoxInnerPadding);
    }

    draw_sprite(g_resources.panelBackground, pos.x, pos.y, width, height);
    draw_wrapped_text(text, pos.x + kBoxInnerPadding, pos.y + kBoxInnerPadding, contentWidth, kFontScale);

    advance_cursor({ 0.0f, height + kContentPadding });
}

ConsoleWindowResult console_window(const ConsoleWindowOptions& options) noexcept
{
    ConsoleWindowResult result{};
    if (!g_frame.ctx)
        return result;

    begin_window(options.title, options.position, options.size);
    if (!g_frame.insideWindow || !g_frame.ctx) {
        end_window();
        return result;
    }

    ensure_resources();

    const float availableWidth = (std::max)(0.0f, options.size.x - 2.0f * kContentPadding);
    const float logHeight = (std::max)(0.0f, options.size.y - 3.0f * kContentPadding - base_line_height(kFontScale));
    const Vec2 logPos = g_frame.cursor;

    if (availableWidth > 0.0f && logHeight > 0.0f) {
        draw_sprite(g_resources.consoleBackground, logPos.x, logPos.y, availableWidth, logHeight);
    }

    const float contentWidth = (std::max)(1.0f, availableWidth - 2.0f * kBoxInnerPadding);
    float penY = logPos.y + kBoxInnerPadding;
    const float maxY = logPos.y + (std::max)(0.0f, logHeight - kBoxInnerPadding);

    if (!options.lines.empty() && logHeight > 0.0f) {
        const std::size_t count = options.lines.size();
        const std::size_t start = (count > options.max_visible_lines)
            ? count - options.max_visible_lines
            : 0;
        for (std::size_t i = start; i < count; ++i) {
            const std::string& line = options.lines[i];
            const float drawn = draw_wrapped_text(line, logPos.x + kBoxInnerPadding, penY, contentWidth, kFontScale);
            const float paragraphGap = (std::max)(0.0f, line_advance_amount(kFontScale) - base_line_height(kFontScale));
            penY += drawn + paragraphGap;
            if (penY > maxY) {
                break;
            }
        }
    }

    set_cursor({ logPos.x, logPos.y + logHeight + kContentPadding });

    if (options.input) {
        const float fieldHeight = base_line_height(kFontScale) + 2.0f * kBoxInnerPadding;
        Vec2 inputSize{ availableWidth, fieldHeight };
        result.input = edit_box(*options.input, inputSize, options.max_input_chars, options.multiline_input);
    }

    end_window();
    return result;
}

void label(std::string_view text) noexcept
{
    if (!g_frame.insideWindow || !g_frame.ctx)
        return;

    draw_text_line(text, g_frame.cursor.x, g_frame.cursor.y, kFontScale);
    advance_cursor({ 0.0f, line_advance_amount(kFontScale) });
}

float line_height() noexcept
{
    try {
        ensure_resources();
    }
    catch (...) {
        return 16.0f;
    }
    return line_advance_amount(kFontScale);
}

float glyph_width() noexcept
{
    try {
        ensure_resources();
    }
    catch (...) {
        return 8.0f * kFontScale;
    }
    return space_advance(kFontScale);
}

std::optional<WidgetBounds> last_button_bounds() noexcept
{
    return g_frame.lastButtonBounds;
}

} // namespace almondnamespace::gui

