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
// ascriptingsystem.cpp
#include "pch.h"

#include "aplatform.hpp"
#include "aengineconfig.hpp"

#include "aenginebindings.hpp"
#include "ascriptingsystem.hpp"
#include "acompiler.hpp"
#include "aenginesystems.hpp"

#include <filesystem>
#include <iostream>

#ifdef _WIN32
#ifndef ALMOND_MAIN_HEADLESS
//    #include <Windows.h>
static HMODULE lastLib = nullptr;
#else
    // windows no main fallback, used for raylib or other contexts that require their own main
static void* lastLib = nullptr;
#endif
#else
#include <dlfcn.h>
static void* lastLib = nullptr;
#endif

using namespace almondnamespace;
using namespace almondnamespace::scripting;
using namespace std::literals;

namespace fs = std::filesystem;
using run_script_fn = void(*)(ScriptScheduler&);

// —————————————————————————————————————————————————————————————————
// Coroutine Task: Compile and Load Script DLL
// —————————————————————————————————————————————————————————————————
static almondnamespace::Task do_load_script(const std::string& scriptName, ScriptScheduler& scheduler, ScriptLoadReport& report) {
    try {
        const fs::path sourcePath = fs::path("src/scripts") / (scriptName + ".ascript.cpp");
        const fs::path dllPath = fs::path("src/scripts") / (scriptName + ".dll");

        report.scheduled.store(true, std::memory_order_relaxed);
        report.log_info("Scheduling script reload for '" + scriptName + "'.");

        if (!fs::exists(sourcePath)) {
            const std::string message = "[script] Source file missing: " + sourcePath.string();
            std::cerr << message << "\n";
            report.log_error(message);
            co_return;
        }

        if (lastLib) {
#ifdef _WIN32
#ifndef ALMOND_MAIN_HEADLESS
            FreeLibrary(lastLib);
#endif
#else
            dlclose(lastLib);
#endif
            lastLib = nullptr;
        }

        if (!compiler::compile_script_to_dll(sourcePath, dllPath)) {
            const std::string message = "[script] Compilation failed: " + sourcePath.string();
            std::cerr << message << "\n";
            report.log_error(message);
            co_return;
        }

        report.compiled.store(true, std::memory_order_relaxed);
        report.log_info("Compiled script '" + scriptName + "' to DLL.");

        if (!fs::exists(dllPath)) {
            const std::string message = "[script] Expected output missing after compilation: " + dllPath.string();
            std::cerr << message << "\n";
            report.log_error(message);
            co_return;
        }

#ifdef _WIN32
#ifndef ALMOND_MAIN_HEADLESS
        lastLib = LoadLibraryA(dllPath.string().c_str());
        if (!lastLib) {
            const std::string message = "[script] LoadLibrary failed: " + dllPath.string();
            std::cerr << message << "\n";
            report.log_error(message);
            co_return;
        }
        auto entry = reinterpret_cast<run_script_fn>(GetProcAddress(lastLib, "run_script"));
#else
        //  auto entry = reinterpret_cast<run_script_fn>("somethingel", "run_script");
#endif
#else
        lastLib = dlopen(dllPath.string().c_str(), RTLD_NOW);
        if (!lastLib) {
            const std::string message = "[script] dlopen failed: " + dllPath.string();
            std::cerr << message << "\n";
            report.log_error(message);
            co_return;
        }
        auto entry = reinterpret_cast<run_script_fn>(dlsym(lastLib, "run_script"));
#endif

        report.dllLoaded.store(true, std::memory_order_relaxed);

#ifndef ALMOND_MAIN_HEADLESS
        if (!entry) {
            const std::string message = "[script] Missing run_script symbol in: " + dllPath.string();
            std::cerr << message << "\n";
            report.log_error(message);
            co_return;
        }

        report.log_info("Executing run_script for '" + scriptName + "'.");
        entry(scheduler);
        report.executed.store(true, std::memory_order_relaxed);
#endif
    }
    catch (const std::exception& e) {
        const std::string message = std::string("[script] Exception during script load: ") + e.what();
        std::cerr << message << "\n";
        report.log_error(message);
    }
    catch (...) {
        const std::string message = "[script] Unknown exception during script load";
        std::cerr << message << "\n";
        report.log_error(message);
    }
    co_return;
}

// —————————————————————————————————————————————————————————————————
// External entry point (engine-facing)
// —————————————————————————————————————————————————————————————————
bool almondnamespace::scripting::load_or_reload_script(const std::string& scriptName, ScriptScheduler& scheduler, ScriptLoadReport* reportPtr) {
    ScriptLoadReport fallbackReport;
    ScriptLoadReport& report = reportPtr ? *reportPtr : fallbackReport;
    report.reset();

    try {
        almondnamespace::Task t = do_load_script(scriptName, scheduler, report);
        auto node = std::make_unique<taskgraph::Node>(std::move(t));
        node->Label = "script:" + scriptName;
        scheduler.AddNode(std::move(node));
        scheduler.Execute();
        scheduler.WaitAll();
        scheduler.PruneFinished();
        return report.succeeded();
    }
    catch (const std::exception& e) {
        const std::string message = std::string("[script] Scheduling exception: ") + e.what();
        std::cerr << message << "\n";
        report.log_error(message);
        return false;
    }
}