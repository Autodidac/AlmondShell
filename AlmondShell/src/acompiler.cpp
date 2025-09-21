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
#include "pch.h"

#include "acompiler.hpp"

#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <iostream>

namespace almond::compiler {

bool compile_script_to_dll(const std::filesystem::path& input, const std::filesystem::path& output) {
    std::vector<std::string> clangArgs = {
        "clang++",
        "-std=c++20",
        "-shared",
        input.string(),
        "-o", output.string(),
        "-Iinclude",
        "-fno-rtti",
        "-fno-exceptions",
        "-O2"
    };

    std::string cmd;
    for (const auto& arg : clangArgs) {
        cmd += arg + " ";
    }

    std::cout << "[compiler] running: " << cmd << std::endl;
    int result = std::system(cmd.c_str());
    if (result != 0) {
        std::cerr << "[compiler] clang++ failed with code: " << result << std::endl;
        return false;
    }

    return true;
}

}
