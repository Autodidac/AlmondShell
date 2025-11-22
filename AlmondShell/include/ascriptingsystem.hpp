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
 //ascriptingsystem.hpp
#pragma once

#include "aplatform.hpp"                // Platform abstraction
#include "ataskgraphwithdot.hpp"       // TaskGraph, Task, Node

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace almondnamespace::scripting {

    /// TaskGraph specialization used to run async scripting jobs
    using ScriptScheduler = taskgraph::TaskGraph;

    /// Thread-safe diagnostics surface for script reload operations.
    struct ScriptLoadReport {
        std::atomic<bool> scheduled{ false };
        std::atomic<bool> compiled{ false };
        std::atomic<bool> dllLoaded{ false };
        std::atomic<bool> executed{ false };
        std::atomic<bool> failed{ false };

        /// Reset all state and forget previously captured log messages.
        inline void reset() {
            scheduled.store(false, std::memory_order_relaxed);
            compiled.store(false, std::memory_order_relaxed);
            dllLoaded.store(false, std::memory_order_relaxed);
            executed.store(false, std::memory_order_relaxed);
            failed.store(false, std::memory_order_relaxed);
            std::lock_guard<std::mutex> lock(messageMutex_);
            messages_.clear();
        }

        /// Append an informational note to the diagnostics stream.
        inline void log_info(const std::string& message) {
            std::lock_guard<std::mutex> lock(messageMutex_);
            messages_.push_back(message);
        }

        /// Append an error note and mark the reload as failed.
        inline void log_error(const std::string& message) {
            failed.store(true, std::memory_order_relaxed);
            std::lock_guard<std::mutex> lock(messageMutex_);
            messages_.push_back(message);
        }

        /// Returns true when all stages succeeded and no failure was recorded.
        inline bool succeeded() const {
            return scheduled.load(std::memory_order_relaxed)
                && compiled.load(std::memory_order_relaxed)
                && dllLoaded.load(std::memory_order_relaxed)
                && executed.load(std::memory_order_relaxed)
                && !failed.load(std::memory_order_relaxed);
        }

        /// Copy the accumulated diagnostic messages.
        inline std::vector<std::string> messages() const {
            std::lock_guard<std::mutex> lock(messageMutex_);
            return messages_;
        }

    private:
        mutable std::mutex messageMutex_;
        std::vector<std::string> messages_;
    };

    /// Asynchronously compiles and reloads a `.ascript.cpp` script as a DLL.
    /// The result is dynamically loaded and executed with the provided scheduler.
    /// Returns true if the task was successfully scheduled.
    bool load_or_reload_script(const std::string& name, ScriptScheduler& scheduler, ScriptLoadReport* report = nullptr);

} // namespace almondnamespace::scripting