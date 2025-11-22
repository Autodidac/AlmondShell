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
 // aversion.hpp
#pragma once

#include <array>
#include <cstdio>
#include <string>
#include <string_view>

namespace almondnamespace 
{
    // Version information as constexpr for compile-time evaluation
    constexpr int major = 0;
    constexpr int minor = 70;
    constexpr int revision = 3;

    inline constexpr std::string_view kEngineName = "Almond Shell";

    // Use inline to ensure that each definition is treated uniquely in different translation units
    inline int GetMajor() { return major; }
    inline int GetMinor() { return minor; }
    inline int GetRevision() { return revision; }

    inline const char* GetEngineName()
    {
        return kEngineName.data();
    }

    inline std::string_view GetEngineNameView()
    {
        return kEngineName;
    }

    inline const char* GetEngineVersion()
    {
        thread_local std::array<char, 32> version_string{};
        std::snprintf(version_string.data(), version_string.size(), "%d.%d.%d", major, minor, revision);
        return version_string.data();
    }

    inline std::string GetEngineVersionString()
    {
        return std::string{ GetEngineVersion() };
    }

} // namespace almondnamespace
