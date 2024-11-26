#pragma once

#include "EventSystem.h"
#include "Exports_DLL.h"
#include "LoadSave.h"
#include "Logger.h"
#include "PluginManager.h"
#include "Scene.h"
#include "SceneSnapshot.h"
#include "ThreadPool.h"
#include "Types.h"
#include "UI_Button.h"
#include "UI_Manager.h"

#include "framework.h"

#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

#ifdef _MSC_VER
    #pragma warning(disable : 4251)
    #pragma warning(disable : 4273)
#endif

namespace almond {

    int GetMajor();
    int GetMinor();
    int GetRevision();

    void RegisterAlmondCallback(std::function<void()> callback);

    class ALMONDSHELL_API AlmondShell {
    public:
        AlmondShell(size_t numThreads, bool running, Scene* scene, size_t maxBufferSize);
        ~AlmondShell();

        static const char* GetEngineVersion();

        void Run();
        void RunWin32Desktop(MSG msg, HACCEL hAccelTable);
        bool IsItRunning() const;
        void SetRunning(bool running);
        void PrintMessage(const std::string& text);

        void PrintOutFPS();
        void UpdateFPS();
        void SetFrameRate(unsigned int targetFPS);

        void SetPlaybackTargetTime(float targetTime);
        void CaptureSnapshot();
        void SaveBinaryState();
        void LoadStateAtTime(float timeStamp);

        static void RegisterCallbackUpdate(std::function<void()> callback) {
            almond::RegisterAlmondCallback(callback);
        }

        float m_fps = 0.0f;

    private:
        almond::plugin::PluginManager m_pluginManager;

        size_t m_maxBufferSize;
        std::deque<SceneSnapshot> m_recentStates;
        std::unique_ptr<almond::Scene> m_scene;
        bool m_running;

        ThreadPool m_jobSystem;
        almond::SaveSystem m_saveSystem;
        std::vector<Event> m_events;

        float m_targetTime = 0.0f;
        void UpdatePlayback();

        void LimitFrameRate(std::chrono::steady_clock::time_point& lastFrame);
        int m_frameCount = 0;
        std::chrono::steady_clock::time_point m_lastSecond;
        unsigned int m_targetFPS = 120;
        bool m_frameLimitingEnabled = false;
        std::chrono::steady_clock::time_point m_lastFrame = std::chrono::steady_clock::now();

        int m_saveIntervalMinutes = 1;

        void Serialize(const std::string& filename, const std::vector<Event>& events);
        void Deserialize(const std::string& filename, std::vector<Event>& events);

        // New method to handle event processing using the EventSystem
        void ProcessEvents();
    };

    extern "C" AlmondShell* CreateAlmondShell(size_t numThreads, bool running, Scene* scene, size_t maxBufferSize);
    extern "C" void Run(AlmondShell& core);
    extern "C" bool IsRunning(AlmondShell& core);
    extern "C" void PrintFPS(AlmondShell& core);
} // namespace almond
