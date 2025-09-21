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
#pragma once
#include "atypes.hpp"  // Ensures almond types are defined, like uint32, uint16, uint64, etc.

#ifdef _WIN32

//#include <Windows.h>  // For Windows-specific HANDLE type
#include "aframework.hpp"

namespace almondnamespace
{
    // Define platform-specific Handle type for Windows
    using Handle = HANDLE;


    // Windows DWORD and WORD definitions adapted for 64-bit compatibility
    using DWORD = uint32;
    using WORD = uint16;

    // Structure with 64-bit compatibility and packed alignment for data transfer
#pragma pack(push, 1)
    struct MyPackedStruct {
        DWORD id;     // 32-bit identifier
        WORD type;    // 16-bit type indicator
        uint32 data;  // 32-bit data field

        uint64 largeData; // Example 64-bit field for 64-bit data compatibility
    };
#pragma pack(pop)  // Restore default packing alignment
}  // namespace almond
#endif