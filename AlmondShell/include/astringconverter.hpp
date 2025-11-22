// astring_convert.hpp
#pragma once

#include "aplatform.hpp"
#include "aengineconfig.hpp"

#include <string>

namespace almondnamespace::text
{
#if defined(_WIN32)
    // UTF-16 (wchar_t) -> UTF-8
    inline std::string narrow_utf8(const std::wstring& wide)
    {
        if (wide.empty()) return {};
        // Query size
        int size = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            wide.data(), static_cast<int>(wide.size()),
            nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string out(static_cast<size_t>(size), '\0');
        // Convert
        int written = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            wide.data(), static_cast<int>(wide.size()),
            out.data(), size, nullptr, nullptr);
        if (written != size) out.resize(static_cast<size_t>((std::max)(written, 0)));
        return out;
    }

    // UTF-8 -> UTF-16 (wchar_t)
    inline std::wstring widen_utf16(const std::string& utf8)
    {
        if (utf8.empty()) return {};
        int size = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            utf8.data(), static_cast<int>(utf8.size()),
            nullptr, 0);
        if (size <= 0) return {};
        std::wstring out(static_cast<size_t>(size), L'\0');
        int written = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            utf8.data(), static_cast<int>(utf8.size()),
            out.data(), size);
        if (written != size) out.resize(static_cast<size_t>((std::max)(written, 0)));
        return out;
    }
#else
    // Portable fallback (C++ facet is deprecated but still widely available).
    // If you later pull in ICU or similar, swap this out.
#include <locale>
#include <codecvt>
    inline std::string narrow_utf8(const std::wstring& wide)
    {
        static thread_local std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
        return conv.to_bytes(wide);
    }
    inline std::wstring widen_utf16(const std::string& utf8)
    {
        static thread_local std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
        return conv.from_bytes(utf8);
    }
#endif
} // namespace almondnamespace::text
