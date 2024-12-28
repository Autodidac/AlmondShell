#pragma once

#include "IPlugin.h"
#include "Logger.h"
#include "PluginConcept.h"

#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#ifdef _WIN32
#include "framework.h"
/*
extern "C" {
    HMODULE __stdcall LoadLibraryA(LPCSTR lpLibFileName);
    void* __stdcall GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
    int __stdcall FreeLibrary(HMODULE hLibModule);
}
*/
using PluginHandle = HMODULE;
#define LoadSharedLibrary(path) LoadLibraryA(path.string().c_str())
#define GetSymbol(handle, name) GetProcAddress(handle, name)
#define CloseLibrary(handle) FreeLibrary(handle)
#else
#include <dlfcn.h>
using PluginHandle = void*;
#define LoadSharedLibrary(path) dlopen(path.string().c_str(), RTLD_LAZY)
#define GetSymbol(handle, name) dlsym(handle, name)
#define CloseLibrary(handle) dlclose(handle)
#endif

namespace almond::plugin {

    // PluginManager class, header-only
    class PluginManager {
    public:
        using PluginFactoryFunc = IPlugin * (*)();

        // Default constructor with optional logFileName
        explicit PluginManager(const std::string& logFileName = "default_log.txt")
            : logger(logFileName) {
        }

        ~PluginManager() {
            UnloadAllPlugins();
        }

        // C++20 use of std::filesystem::path and ranges for efficient operations
        bool LoadPlugin(const std::filesystem::path& path) {
            logger.log("Attempting to load plugin: " + path.string());

            // Modern handling of the shared library loading
            PluginHandle handle = LoadSharedLibrary(path);
            if (!handle) {
                logger.log("ERROR: Failed to load plugin: " + path.string());
                return false;
            }

            // Use reinterpret_cast to call the function from the plugin
            auto factory = reinterpret_cast<PluginFactoryFunc>(GetSymbol(handle, "CreatePlugin"));
            if (!factory) {
                logger.log("ERROR: Missing entry point in plugin: " + path.string());
                CloseLibrary(handle);
                return false;
            }

            // Using smart pointer to manage plugin instance
            std::unique_ptr<IPlugin> plugin(factory());
            if (plugin) {
                plugin->Initialize();
                plugins.emplace_back(std::move(plugin), handle);
                logger.log("Successfully loaded plugin: " + path.string());
                return true;
            }

            CloseLibrary(handle);
            logger.log("ERROR: Failed to create plugin instance: " + path.string());
            return false;
        }

        // Unloads all plugins
        void UnloadAllPlugins() {
            logger.log("Unloading all plugins...");

            // Reverse iteration with std::ranges for modern loop control
            for (auto& [plugin, handle] : plugins | std::views::reverse) {
                if (plugin) {
                    logger.log("Shutting down plugin...");
                    plugin->Shutdown();
                }

                logger.log("Destroying plugin instance...");
                plugin.reset();

                if (handle) {
                    logger.log("Closing library handle...");
                    CloseLibrary(handle);
                }
            }

            plugins.clear();
            logger.log("All plugins unloaded.");
        }

    private:
        std::vector<std::pair<std::unique_ptr<IPlugin>, PluginHandle>> plugins;
        almond::Logger logger; // Logger instance
    };

} // namespace almond::plugin
