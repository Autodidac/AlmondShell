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

        std::error_code ec;
        if (!fs::exists(folder, ec) || ec) {
            return result;
        }

        for (const auto& entry : fs::directory_iterator(folder, ec)) {
            if (ec) {
                break;
            }

            if (entry.is_regular_file() && entry.path().extension() == ".cpp") {
                file_state fs;
                fs.path = entry.path().string();

                auto last_write = fs::last_write_time(entry, ec);
                if (ec) {
                    ec.clear();
                    continue;
                }

                fs.last_write_time = last_write.time_since_epoch().count();
                fs.hash = fake_hash(fs.path);
                fs.dirty = false;
                result.push_back(std::move(fs));
            }
        }
        return result;
    }

    void scan_and_mark_changes(std::vector<file_state>& files) {
        for (auto& f : files) {
            std::error_code ec;
            auto write_time = fs::last_write_time(f.path, ec);
            if (ec) {
                f.last_write_time = 0;
                f.hash = 0;
                f.dirty = true;
                continue;
            }

            auto new_time = write_time.time_since_epoch().count();
            if (new_time != f.last_write_time) {
                f.last_write_time = new_time;
                f.dirty = true;
            }
        }
    }
}
