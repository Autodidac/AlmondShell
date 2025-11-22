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
 // afontrenderer.hpp
#pragma once

#include "aplatform.hpp"
// #include "aassetsystem.hpp" // uncomment when asset system is ready
#include "acontext.hpp"
#include "aatlastexture.hpp"
#include "aatlasmanager.hpp"
#include "aspritehandle.hpp"
#include "agui.hpp"

#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <span>

namespace almondnamespace::core
{
    struct Context;
}

namespace almondnamespace::font
{
    struct FontMetrics
    {
        float ascent = 0.0f;
        float descent = 0.0f;
        float lineGap = 0.0f;
        float lineHeight = 0.0f;
        float averageAdvance = 0.0f;
        float maxAdvance = 0.0f;
        float spaceAdvance = 0.0f;
    };

    struct UVRect
    {
        ui::vec2 top_left{};
        ui::vec2 bottom_right{};
    };

    struct Glyph
    {
        UVRect uv{};
        ui::vec2 size_px{};     // Glyph pixel size
        ui::vec2 offset_px{};   // Offset from baseline in pixels
        float advance{};        // Cursor advance in pixels after rendering this glyph
        SpriteHandle handle{};  // Sprite handle for rendering this glyph slice
    };

    struct FontAsset
    {
        std::string name;
        float size_pt{};

        std::unordered_map<char32_t, Glyph> glyphs;
        AtlasEntry atlas; // lightweight handle to the glyph atlas texture
        int atlas_index = -1;
        FontMetrics metrics{};
        std::unordered_map<std::uint64_t, float> kerningPairs{};

        [[nodiscard]] float get_kerning(char32_t left, char32_t right) const noexcept
        {
            if (kerningPairs.empty())
                return 0.0f;

            const std::uint64_t key = (static_cast<std::uint64_t>(left) << 32)
                | static_cast<std::uint64_t>(right);
            const auto it = kerningPairs.find(key);
            if (it == kerningPairs.end())
                return 0.0f;
            return it->second;
        }
    };

    class FontRenderer
    {
    public:
        // Load and bake font from TTF file into a glyph atlas
        // Returns false on failure, true on success
        bool load_font(const std::string& name, const std::string& ttf_path, float size_pt)
        {
            FontAsset asset{};
            asset.name = name;
            asset.size_pt = size_pt;

            std::vector<std::pair<char32_t, BakedGlyph>> baked_glyphs{};
            Texture raw_texture{};
            FontMetrics metrics{};

            std::unordered_map<std::uint64_t, float> kerning_pairs{};

            if (!load_and_bake_font(ttf_path, size_pt, baked_glyphs, metrics, kerning_pairs, raw_texture))
            {
                std::cerr << "[FontRenderer] Failed to bake font '" << name << "' from '" << ttf_path << "'\n";
                return false;
            }

            if (raw_texture.empty())
            {
                std::cerr << "[FontRenderer] Baked texture for font '" << name << "' is empty\n";
                return false;
            }

            raw_texture.name = name;

            static std::mutex atlas_mutex;
            const std::string atlas_name = "font_atlas";
            almondnamespace::atlasmanager::create_atlas({
                .name = atlas_name,
                .width = 2048,
                .height = 2048,
                .generate_mipmaps = false
                });

            auto* registrar = almondnamespace::atlasmanager::get_registrar(atlas_name);
            if (!registrar)
            {
                std::cerr << "[FontRenderer] Missing registrar for atlas '" << atlas_name << "'\n";
                return false;
            }

            TextureAtlas& atlas = registrar->atlas;

            std::optional<AtlasEntry> maybe_entry;
            {
                std::lock_guard<std::mutex> lock(atlas_mutex);
                maybe_entry = atlas.add_entry(name, raw_texture);
            }

            if (!maybe_entry)
            {
                std::cerr << "[FontRenderer] Failed to pack font '" << name << "' into shared atlas\n";
                return false;
            }

            const AtlasEntry& atlas_entry = *maybe_entry;
            const float inv_entry_width = atlas_entry.region.width > 0 ?
                1.0f / static_cast<float>(atlas_entry.region.width) : 0.0f;
            const float inv_entry_height = atlas_entry.region.height > 0 ?
                1.0f / static_cast<float>(atlas_entry.region.height) : 0.0f;
            const float entry_uv_width = atlas_entry.region.uv_width();
            const float entry_uv_height = atlas_entry.region.uv_height();

            float total_advance = 0.0f;
            std::size_t advance_count = 0;
            float max_advance = 0.0f;

            for (auto& [codepoint, baked] : baked_glyphs)
            {
                Glyph glyph = std::move(baked.glyph);

                const float u0 = atlas_entry.region.u1
                    + static_cast<float>(baked.x0) * inv_entry_width * entry_uv_width;
                const float v0 = atlas_entry.region.v1
                    + static_cast<float>(baked.y0) * inv_entry_height * entry_uv_height;
                const float u1 = atlas_entry.region.u1
                    + static_cast<float>(baked.x1) * inv_entry_width * entry_uv_width;
                const float v1 = atlas_entry.region.v1
                    + static_cast<float>(baked.y1) * inv_entry_height * entry_uv_height;

                glyph.uv.top_left = { u0, v0 };
                glyph.uv.bottom_right = { u1, v1 };

                const int glyph_width = baked.x1 - baked.x0;
                const int glyph_height = baked.y1 - baked.y0;

                if (glyph_width > 0 && glyph_height > 0)
                {
                    const int slice_x = static_cast<int>(atlas_entry.region.x) + baked.x0;
                    const int slice_y = static_cast<int>(atlas_entry.region.y) + baked.y0;
                    const std::string glyph_name = name + "_pt" + std::to_string(size_pt)
                        + "_cp" + std::to_string(static_cast<uint32_t>(codepoint));

                    std::optional<AtlasEntry> glyph_entry;
                    {
                        std::lock_guard<std::mutex> lock(atlas_mutex);
                        glyph_entry = atlas.add_slice_entry(glyph_name, slice_x, slice_y, glyph_width, glyph_height);
                    }

                    if (glyph_entry)
                    {
                        glyph.handle = SpriteHandle{
                            static_cast<uint32_t>(glyph_entry->index),
                            0u,
                            static_cast<uint32_t>(atlas.get_index()),
                            static_cast<uint32_t>(glyph_entry->index)
                        };
                    }
                }
                asset.glyphs.emplace(codepoint, std::move(glyph));

                const Glyph& stored_glyph = asset.glyphs.at(codepoint);
                total_advance += stored_glyph.advance;
                max_advance = (std::max)(max_advance, stored_glyph.advance);
                ++advance_count;
                if (codepoint == U' ')
                {
                    metrics.spaceAdvance = stored_glyph.advance;
                }
            }

            if (advance_count > 0)
            {
                metrics.averageAdvance = total_advance / static_cast<float>(advance_count);
            }
            metrics.maxAdvance = (std::max)(metrics.maxAdvance, max_advance);
            if (metrics.spaceAdvance <= 0.0f)
            {
                metrics.spaceAdvance = metrics.averageAdvance;
            }

            asset.atlas = atlas_entry;
            asset.atlas_index = atlas.get_index();
            asset.metrics = metrics;
            asset.kerningPairs = std::move(kerning_pairs);
            almondnamespace::atlasmanager::ensure_uploaded(atlas);
            loaded_fonts_.emplace(name, std::move(asset));
            return true;
        }

        [[nodiscard]] const FontAsset* get_font(const std::string& name) const noexcept
        {
            auto it = loaded_fonts_.find(name);
            if (it == loaded_fonts_.end())
                return nullptr;
            return &it->second;
        }


        // Render UTF-32 text at pixel coordinates in screen space using an explicit atlas span
        void render_text(core::Context& ctx,
            std::span<const TextureAtlas* const> atlases,
            const std::string& font_name,
            const std::u32string& text,
            ui::vec2 pos_px) const
        {
            auto it = loaded_fonts_.find(font_name);
            if (it == loaded_fonts_.end())
                return; // font not loaded, silently drop text (optionally log warning)

            const FontAsset& font = it->second;
            ui::vec2 cursor = pos_px;
            const float lineHeight = (font.metrics.lineHeight > 0.0f)
                ? font.metrics.lineHeight
                : (font.metrics.ascent + font.metrics.descent);
            const float fallbackAdvance = (font.metrics.spaceAdvance > 0.0f)
                ? font.metrics.spaceAdvance
                : font.metrics.averageAdvance;

            char32_t previous = U'\0';
            for (char32_t ch : text)
            {
                if (ch == U'\n')
                {
                    cursor.x = pos_px.x;
                    cursor.y += lineHeight;
                    previous = U'\0';
                    continue;
                }

                float kern = 0.0f;
                if (previous != U'\0')
                    kern = font.get_kerning(previous, ch);

                auto glyph_it = font.glyphs.find(ch);
                if (glyph_it == font.glyphs.end())
                {
                    cursor.x += (fallbackAdvance > 0.0f) ? fallbackAdvance + kern : kern;
                    previous = U'\0';
                    continue;
                }

                const Glyph& g = glyph_it->second;
                cursor.x += kern;

                if (g.handle.is_valid())
                {
                    ui::vec2 top_left = cursor + g.offset_px;
                    ctx.draw_sprite_safe(g.handle, atlases, top_left.x, top_left.y, g.size_px.x, g.size_px.y);
                }

                cursor.x += g.advance;
                previous = ch;
            }
        }

        // Convenience overload that gathers atlases from the global atlas manager
        void render_text(core::Context& ctx,
            const std::string& font_name,
            const std::u32string& text,
            ui::vec2 pos_px) const
        {
            const auto& atlas_vec = almondnamespace::atlasmanager::get_atlas_vector();
            if (atlas_vec.empty())
                return;

            std::span<const TextureAtlas* const> atlas_span(atlas_vec.data(), atlas_vec.size());
            render_text(ctx, atlas_span, font_name, text, pos_px);
        }

    private:
        struct BakedGlyph
        {
            Glyph glyph{};
            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
        };

        bool load_and_bake_font(const std::string& ttf_path,
            float size_pt,
            std::vector<std::pair<char32_t, BakedGlyph>>& out_glyphs,
            FontMetrics& out_metrics,
            std::unordered_map<std::uint64_t, float>& out_kerning,
            Texture& out_texture);

        std::unordered_map<std::string, FontAsset> loaded_fonts_{};
    };

} // namespace almondnamespace::font
