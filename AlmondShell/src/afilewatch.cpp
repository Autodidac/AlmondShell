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

#include "afilewatch.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace almondnamespace 
{
    static std::uint64_t fake_hash(const std::string& path) {
        return std::hash<std::string>{}(path);
    }

    std::vector<file_state> get_file_states(const std::string& folder) {
        std::vector<file_state> result;
        for (const auto& entry : fs::directory_iterator(folder)) {
            if (entry.path().extension() == ".cpp") {
                file_state fs;
                fs.path = entry.path().string();
                fs.last_write_time = fs::last_write_time(entry).time_since_epoch().count();
                fs.hash = fake_hash(fs.path);
                fs.dirty = false;
                result.push_back(fs);
            }
        }
        return result;
    }

    void scan_and_mark_changes(std::vector<file_state>& files) {
        for (auto& f : files) {
            auto new_time = fs::last_write_time(f.path).time_since_epoch().count();
            if (new_time != f.last_write_time) {
                f.last_write_time = new_time;
                f.dirty = true;
            }
        }
    }
}
