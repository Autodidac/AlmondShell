
#include "pch.h"

#include "aplatform.hpp"
#include "aengineconfig.hpp"

#include "afontrenderer.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <vector>

namespace almondnamespace::font
{
    namespace
    {
        [[nodiscard]] std::vector<unsigned char> read_file_binary(const std::string& path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
                return {};

            file.seekg(0, std::ios::end);
            const std::streamsize size = file.tellg();
            if (size <= 0)
                return {};
            file.seekg(0, std::ios::beg);

            std::vector<unsigned char> buffer(static_cast<std::size_t>(size));
            if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
                return {};

            return buffer;
        }
    }

    bool FontRenderer::load_and_bake_font(const std::string& ttf_path,
        float size_pt,
        std::vector<std::pair<char32_t, BakedGlyph>>& out_glyphs,
        FontMetrics& out_metrics,
        std::unordered_map<std::uint64_t, float>& out_kerning,
        Texture& out_texture)
    {
        out_glyphs.clear();
        out_kerning.clear();
        out_texture.clear();
        out_texture.channels = 4;
        out_texture.name = ttf_path;
        out_metrics = FontMetrics{};

        if (size_pt <= 0.0f)
        {
            std::cerr << "[FontRenderer] Invalid font size '" << size_pt << "' requested for '" << ttf_path << "'\n";
            return false;
        }

        auto font_buffer = read_file_binary(ttf_path);
        if (font_buffer.empty())
        {
            std::cerr << "[FontRenderer] Unable to read font file '" << ttf_path << "'\n";
            return false;
        }

        int font_offset = stbtt_GetFontOffsetForIndex(font_buffer.data(), 0);
        if (font_offset < 0)
        {
            std::cerr << "[FontRenderer] Invalid font offset for '" << ttf_path << "'\n";
            return false;
        }

        stbtt_fontinfo font{};
        if (!stbtt_InitFont(&font, font_buffer.data(), font_offset))
        {
            std::cerr << "[FontRenderer] Failed to initialise font info for '" << ttf_path << "'\n";
            return false;
        }

        int raw_ascent = 0;
        int raw_descent = 0;
        int raw_line_gap = 0;
        stbtt_GetFontVMetrics(&font, &raw_ascent, &raw_descent, &raw_line_gap);
        const float scale = stbtt_ScaleForPixelHeight(&font, size_pt);
        out_metrics.ascent = static_cast<float>(raw_ascent) * scale;
        out_metrics.descent = static_cast<float>(-raw_descent) * scale;
        out_metrics.lineGap = static_cast<float>(raw_line_gap) * scale;
        out_metrics.lineHeight = out_metrics.ascent + out_metrics.descent + out_metrics.lineGap;

        constexpr int pack_width = 1024;
        constexpr int pack_height = 1024;
        std::vector<unsigned char> mono_bitmap(static_cast<std::size_t>(pack_width) * pack_height, 0);

        stbtt_pack_context pack_context{};
        if (!stbtt_PackBegin(&pack_context, mono_bitmap.data(), pack_width, pack_height, pack_width, 1, nullptr))
        {
            std::cerr << "[FontRenderer] Failed to begin packing for font '" << ttf_path << "'\n";
            return false;
        }
        stbtt_PackSetOversampling(&pack_context, 2, 2);

        const std::array<std::pair<int, int>, 2> ranges_info{ {
            {32, 126},        // Basic Latin
            {160, 255}        // Latin-1 Supplement
        } };

        std::size_t total_chars = 0;
        for (const auto& range : ranges_info)
            total_chars += static_cast<std::size_t>(range.second - range.first + 1);

        std::vector<stbtt_packedchar> packed_chars(total_chars);
        std::vector<stbtt_pack_range> pack_ranges(ranges_info.size());

        std::size_t packed_offset = 0;
        for (std::size_t i = 0; i < ranges_info.size(); ++i)
        {
            const auto [first_codepoint, last_codepoint] = ranges_info[i];
            const int glyph_count = last_codepoint - first_codepoint + 1;

            pack_ranges[i].font_size = size_pt;
            pack_ranges[i].first_unicode_codepoint_in_range = first_codepoint;
            pack_ranges[i].array_of_unicode_codepoints = nullptr;
            pack_ranges[i].num_chars = glyph_count;
            pack_ranges[i].chardata_for_range = packed_chars.data() + packed_offset;
            pack_ranges[i].h_oversample = 2;
            pack_ranges[i].v_oversample = 2;

            packed_offset += static_cast<std::size_t>(glyph_count);
        }

        if (!stbtt_PackFontRanges(&pack_context, font_buffer.data(), 0, pack_ranges.data(), static_cast<int>(pack_ranges.size())))
        {
            stbtt_PackEnd(&pack_context);
            std::cerr << "[FontRenderer] Failed to pack glyph ranges for font '" << ttf_path << "'\n";
            return false;
        }

        stbtt_PackEnd(&pack_context);

        int max_x1 = 0;
        int max_y1 = 0;
        for (const auto& packed : packed_chars)
        {
            max_x1 = (std::max)(max_x1, static_cast<int>(packed.x1));
            max_y1 = (std::max)(max_y1, static_cast<int>(packed.y1));
        }

        if (max_x1 <= 0 || max_y1 <= 0)
        {
            std::cerr << "[FontRenderer] Packed bitmap for font '" << ttf_path << "' is empty\n";
            return false;
        }

        max_x1 = (std::min)(max_x1, pack_width);
        max_y1 = (std::min)(max_y1, pack_height);

        out_texture.width = static_cast<std::uint32_t>(max_x1);
        out_texture.height = static_cast<std::uint32_t>(max_y1);
        out_texture.channels = 4;
        out_texture.pixels.assign(static_cast<std::size_t>(out_texture.width) * out_texture.height * 4, 0);

        for (std::uint32_t y = 0; y < out_texture.height; ++y)
        {
            for (std::uint32_t x = 0; x < out_texture.width; ++x)
            {
                const unsigned char alpha = mono_bitmap[static_cast<std::size_t>(y) * pack_width + x];
                const std::size_t idx = (static_cast<std::size_t>(y) * out_texture.width + x) * 4;
                out_texture.pixels[idx + 0] = 255;
                out_texture.pixels[idx + 1] = 255;
                out_texture.pixels[idx + 2] = 255;
                out_texture.pixels[idx + 3] = alpha;
            }
        }

        out_glyphs.reserve(total_chars);
        packed_offset = 0;
        for (std::size_t range_index = 0; range_index < ranges_info.size(); ++range_index)
        {
            const auto [first_codepoint, last_codepoint] = ranges_info[range_index];
            const int glyph_count = last_codepoint - first_codepoint + 1;
            const stbtt_packedchar* range_chars = packed_chars.data() + packed_offset;

            for (int glyph_index = 0; glyph_index < glyph_count; ++glyph_index)
            {
                const stbtt_packedchar& packed = range_chars[glyph_index];

                BakedGlyph baked{};
                baked.glyph.size_px = {
                    static_cast<float>(packed.x1 - packed.x0),
                    static_cast<float>(packed.y1 - packed.y0)
                };
                baked.glyph.offset_px = { packed.xoff, packed.yoff };
                baked.glyph.advance = packed.xadvance;
                baked.x0 = packed.x0;
                baked.y0 = packed.y0;
                baked.x1 = packed.x1;
                baked.y1 = packed.y1;

                out_metrics.maxAdvance = (std::max)(out_metrics.maxAdvance, baked.glyph.advance);
                if (first_codepoint + glyph_index == 32)
                {
                    out_metrics.spaceAdvance = baked.glyph.advance;
                }

                out_glyphs.emplace_back(static_cast<char32_t>(first_codepoint + glyph_index), std::move(baked));
            }

            packed_offset += static_cast<std::size_t>(glyph_count);
        }

        if (!out_glyphs.empty())
        {
            out_kerning.reserve(out_glyphs.size() * 4);
            for (const auto& [left_cp, _] : out_glyphs)
            {
                for (const auto& [right_cp, __] : out_glyphs)
                {
                    const int kern = stbtt_GetCodepointKernAdvance(&font,
                        static_cast<int>(left_cp),
                        static_cast<int>(right_cp));
                    if (kern == 0)
                        continue;

                    const float kern_px = static_cast<float>(kern) * scale;
                    const std::uint64_t key = (static_cast<std::uint64_t>(left_cp) << 32)
                        | static_cast<std::uint64_t>(right_cp);
                    out_kerning[key] = kern_px;
                }
            }
        }

        return true;
    }
}
