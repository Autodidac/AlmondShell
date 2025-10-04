﻿/**************************************************************
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

#include "abuildsystem.hpp"
#include "aupdateconfig.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <system_error>

namespace almondnamespace
{
    namespace updater
    {
        // 🔄 **Replace binary with the new compiled version**
        inline void replace_binary_from_script(const std::string& new_binary) {
            std::cout << "[INFO] Replacing current binary...\n";

#if defined(_WIN32)
            //clean_up_build_files();

            std::ofstream batchFile(REPLACE_RUNNING_EXE_SCRIPT_NAME());
            if (!batchFile) {
                std::cerr << "[ERROR] Failed to create batch file for self-replacement.\n";
                return;
            }

            batchFile << "@echo off\n"
                << ":retry\n"
                << "timeout /t 2 >nul\n"
                << "taskkill /IM updater.exe /F >nul 2>&1\n"
                << "timeout /t 1 >nul\n"
                << "del updater.exe >nul 2>&1\n"
                << "if exist updater.exe (\n"
                << "    echo [ERROR] File lock detected. Retrying...\n"
                << "    timeout /t 1 >nul\n"
                << "    goto retry\n"
                << ")\n"
                << "rename \"" << new_binary << "\" \"updater.exe\"\n"
                << "if exist updater.exe (\n"
                << "    echo [INFO] Update successful! Restarting updater...\n"
                << "    timeout /t 4 > nul\n"
                << "    start updater.exe\n"
                << "    exit\n"
                << ") else (\n"
                << "    echo [ERROR] Failed to rename new binary. Update aborted!\n"
                << "    exit\n"
                << ")\n";

            batchFile.close();

            std::cout << "[INFO] Running replacement script...\n";
            system(("start /min " + std::string(REPLACE_RUNNING_EXE_SCRIPT_NAME())).c_str());
            exit(0);

#elif defined(__linux__) || defined(__APPLE__)
            clean_up_build_files();

            std::ofstream scriptFile("replace_and_restart.sh");
            if (!scriptFile) {
                std::cerr << "[ERROR] Failed to create shell script for self-replacement.\n";
                return;
            }

            scriptFile << "#!/bin/sh\n"
                << "sleep 2\n"
                << "pkill -f updater\n"
                << "sleep 1\n"
                << "if pgrep -f updater; then\n"
                << "    echo \"[WARNING] Process still running, forcing kill...\"\n"
                << "    sleep 1\n"
                << "    pkill -9 -f updater\n"
                << "fi\n"
                << "if [ -f updater ]; then\n"
                << "    rm -f updater\n"
                << "fi\n"
                << "mv \"" << new_binary << "\" updater\n"
                << "chmod +x updater\n"
                << "echo \"[INFO] Update successful! Restarting updater...\"\n"
                << "./updater &\n"
                << "rm -- \"$0\"\n";

            scriptFile.close();

            std::cout << "[INFO] Running replacement script...\n";
            system("chmod +x replace_and_restart.sh && nohup ./replace_and_restart.sh &");
            exit(0);
#endif
        }

        inline bool move_directory_contents(const std::filesystem::path& source,
                                            const std::filesystem::path& destination)
        {
            namespace fs = std::filesystem;

            if (!fs::exists(source)) {
                std::cerr << "[ERROR] Source directory does not exist: " << source << '\n';
                return false;
            }

            std::error_code ec;
            if (!fs::exists(destination)) {
                fs::create_directories(destination, ec);
                if (ec) {
                    std::cerr << "[ERROR] Failed to create destination directory: "
                              << destination << " (" << ec.message() << ")\n";
                    return false;
                }
            }

            for (const auto& entry : fs::directory_iterator(source)) {
                const auto target = destination / entry.path().filename();

                std::error_code remove_ec;
                fs::remove_all(target, remove_ec);
                if (remove_ec && remove_ec != std::errc::no_such_file_or_directory) {
                    std::cerr << "[ERROR] Failed to remove existing path: " << target
                              << " (" << remove_ec.message() << ")\n";
                    return false;
                }

                std::error_code rename_ec;
                fs::rename(entry.path(), target, rename_ec);
                if (rename_ec) {
                    std::error_code copy_ec;
                    fs::copy(entry.path(), target,
                             fs::copy_options::recursive | fs::copy_options::overwrite_existing,
                             copy_ec);
                    if (copy_ec) {
                        std::cerr << "[ERROR] Failed to copy " << entry.path() << " → "
                                  << target << " (" << copy_ec.message() << ")\n";
                        return false;
                    }

                    std::error_code cleanup_ec;
                    fs::remove_all(entry.path(), cleanup_ec);
                    if (cleanup_ec && cleanup_ec != std::errc::no_such_file_or_directory) {
                        std::cerr << "[WARNING] Failed to clean up temporary path: "
                                  << entry.path() << " (" << cleanup_ec.message() << ")\n";
                    }
                }
            }

            return true;
        }

        // 🔄 **Replace the Current Binary**
        inline void replace_binary(const std::string& new_binary) {
            std::cout << "[INFO] Replacing binary...\n";
            clean_up_build_files();
#if defined(_WIN32)
            replace_binary_from_script(new_binary);
            //system(("move /Y " + new_binary + " updater.exe").c_str());
#elif defined(__linux__) || defined(__APPLE__)
            replace_binary_from_script(new_binary);
            // system(("mv " + new_binary + " updater").c_str());
#endif
            return;
        }

        // 🔍 **Check for Updates**
        bool check_for_updates(const std::string& remote_config_url) {
            std::cout << "[INFO] Checking for updates...\n";

            std::string temp_config_path = "temp_config.hpp";

            // ✅ Download `config.hpp` using `wget` or `curl`, but don't keep it permanently
#if defined(_WIN32)
            std::string command = "curl -L --fail --silent --show-error -o \"" + temp_config_path + "\" \"" + remote_config_url + "\"";
#else
            std::string command = "wget --quiet --output-document=" + temp_config_path + " " + remote_config_url;
#endif

            if (system(command.c_str()) != 0) {
                std::cerr << "[ERROR] Failed to download config.hpp!\n";
                return false;
            }

            // ✅ Read `config.hpp` from the temporary file
            std::ifstream config_file(temp_config_path);
            if (!config_file) {
                std::cerr << "[ERROR] Failed to open downloaded config.hpp for reading.\n";
                return false;
            }

            std::string line, latest_version;
            std::regex version_regex(R"(inline\s+std::string\s+PROJECT_VERSION\s*=\s*\"([0-9]+\.[0-9]+\.[0-9]+)\";)");

            while (std::getline(config_file, line)) {
                std::smatch match;
                if (std::regex_search(line, match, version_regex)) {
                    latest_version = match[1]; // Extract version number
                    break;
                }
            }

            config_file.close();

            // ✅ Delete temporary file immediately after reading
#if defined(_WIN32)
            system(("del /F /Q " + temp_config_path + " >nul 2>&1").c_str());
#else
            system(("rm -f " + temp_config_path).c_str());
#endif

            if (latest_version.empty()) {
                std::cerr << "[ERROR] Failed to extract version from config.hpp!\n";
                return false;
            }

            std::cout << "[INFO] Local Version: " << PROJECT_VERSION << "\n";
            std::cout << "[INFO] Remote Version: " << latest_version << "\n";

            // ✅ Compare remote version with local version
            return latest_version != PROJECT_VERSION;
        }

        // 🚀 **Install Updater from Binary**
        inline bool install_from_binary(const std::string& binary_url) {
            std::cout << "[INFO] Installing from precompiled binary...\n";

            const std::string output_binary = OUTPUT_BINARY();

            if (!download_file(binary_url, output_binary)) {
                std::cerr << "[ERROR] Failed to download precompiled binary!\n";
                return false;
            }
            replace_binary(output_binary);

            return true;
        }

        // 🚀 **Install Updater from Source**
        inline void install_from_source(const std::string& binary_url) {
            std::cout << "[INFO] Installing from source...\n";

            if (!setup_7zip()) {
                std::cerr << "[ERROR] 7-Zip installation failed. Cannot continue.\n";
                return;
            }

            if (!download_file(PROJECT_SOURCE_URL(), "source_code.zip") || !extract_archive("source_code.zip")) {
                std::cerr << "[ERROR] Failed to download or extract source archive.\n";
                return;
            }

            std::string extracted_folder = REPO + "-main";

            // ✅ Debug: Check if files exist before moving
            std::cout << "[DEBUG] Listing extracted files before moving:\n";
#if defined(_WIN32)
            system(("dir /s /b " + extracted_folder).c_str());
#else
            system(("ls -l " + extracted_folder).c_str());
#endif

            const std::filesystem::path extracted_path{ extracted_folder };
            const auto destination = std::filesystem::current_path();

            if (!move_directory_contents(extracted_path, destination)) {
                std::cerr << "[ERROR] Failed to move extracted files." << std::endl;
                return;
            }

            std::error_code cleanup_ec;
            std::filesystem::remove_all(extracted_path, cleanup_ec);
            if (cleanup_ec && cleanup_ec != std::errc::no_such_file_or_directory) {
                std::cerr << "[WARNING] Failed to delete extracted folder: "
                          << extracted_folder << " (" << cleanup_ec.message() << ")" << std::endl;
            }

            if (!setup_llvm_clang() || !setup_ninja()) {
                std::cerr << "[ERROR] LLVM/Ninja installation failed. Aborting.\n";
                return;
            }

            if (!generate_ninja_build_file()) {
                std::cerr << "[ERROR] Failed to generate Ninja build file. Aborting.\n";
                return;
            }

            std::cout << "[INFO] Running Ninja...\n";
            if (system("ninja") != 0) {
                std::cerr << "[ERROR] Compilation failed! Update aborted.\n";
                return;
            }

            // you can reverse the redunant fallback order from "binary to source" to "source to binary" in download/usage order
            // by using this and altering the update_from_source() function differently, add binary_url to install_from_source(const std::string& binary_url) 
            //install_from_binary(binary_url);
            replace_binary(OUTPUT_BINARY());
        }

        // 🔄 **Update from Source (Minimal)**
        void update_project(const std::string& source_url, const std::string& binary_url) {

            //    if (!check_for_updates(source_url)) {
            //#if defined(_WIN32)
            //        system("cls");  // Windows
            //#else
            //        system("clear"); // Linux/macOS
            //#endif
            //        std::cout << "[INFO] No updates available.\n";
            //        return;
            //    }

                // get rid of this line if you're looking to reverse redundant fallback order
            if (!install_from_binary(binary_url))
            {
                std::cout << "[INFO] Updating from source...\n";
                install_from_source(binary_url);
            }
        }
    }
}