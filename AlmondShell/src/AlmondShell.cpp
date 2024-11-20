#include "AlmondShell.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <shared_mutex>

// Mutex for callback thread safety
std::shared_mutex callbackMutex;
std::function<void()> m_updateCallback;

void RegisterAlmondCallback(const std::function<void()> callback) {
    std::unique_lock lock(callbackMutex);
    m_updateCallback = callback;
}

// main engine core
const int major = 0;
// minor features, major updates, breaking compatibility changes
const int minor = 0;
// minor bug fixes, alterations, refactors, updates
const int revision = 4;

/*
static const std::string version_string = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(revision);

const char* GetVersionString()
{
    std::cout << version_string << std::endl;
    return version_string.c_str();
}
*/
namespace almond {

    AlmondShell::AlmondShell(size_t numThreads, bool running, Scene* scene, size_t maxBufferSize)
        : m_jobSystem(numThreads), m_running(running), m_scene(scene),
        m_frameCount(0), m_lastSecond(std::chrono::steady_clock::now()), m_maxBufferSize(maxBufferSize) {}

    void AlmondShell::CaptureSnapshot() {
        if (m_recentStates.size() >= m_maxBufferSize) {
            m_recentStates.pop_front();
        }

        auto currentSceneState = m_scene->clone();
        float timestamp = std::chrono::duration<float>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        m_recentStates.emplace_back(timestamp, std::move(currentSceneState));
    }

    void AlmondShell::SaveBinaryState() {
        if (!m_recentStates.empty()) {
            const SceneSnapshot& snapshot = m_recentStates.back();

            try {
                std::ofstream ofs("savegame.dat", std::ios::binary | std::ios::app);
                if (!ofs) throw std::ios_base::failure("Failed to open save file.");

                ofs.write(reinterpret_cast<const char*>(&snapshot), sizeof(SceneSnapshot));
                ofs.close();
            }
            catch (const std::ios_base::failure& e) {
                std::cerr << e.what() << std::endl;
            }
        }
    }

    void AlmondShell::SetPlaybackTargetTime(float targetTime) {
        m_targetTime = targetTime;
    }

    void AlmondShell::UpdatePlayback() {
        if (!m_recentStates.empty() && m_recentStates.front().timeStamp <= m_targetTime) {
            for (const auto& snapshot : m_recentStates) {
                if (snapshot.timeStamp >= m_targetTime) {
                    m_scene = std::make_unique<Scene>(std::move(*snapshot.currentState));
                    break;
                }
            }
        }
        else {
            LoadStateAtTime(m_targetTime);
        }
    }

    void AlmondShell::LoadStateAtTime(float timeStamp) {
        try {
            std::ifstream ifs("savegame.dat", std::ios::binary);
            if (!ifs) throw std::ios_base::failure("Failed to open save file.");

            SceneSnapshot snapshot;
            while (ifs.read(reinterpret_cast<char*>(&snapshot), sizeof(SceneSnapshot))) {
                if (snapshot.timeStamp >= timeStamp) {
                    m_scene = std::make_unique<Scene>(std::move(*snapshot.currentState));
                    break;
                }
            }
        }
        catch (const std::ios_base::failure& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    void AlmondShell::RunWin32Desktop(MSG msg, HACCEL hAccelTable) {
        auto lastFrame = std::chrono::steady_clock::now();
        auto lastSave = std::chrono::steady_clock::now();

        while (m_running) {
            auto currentTime = std::chrono::steady_clock::now();
            UpdateFPS();

            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }

                if (msg.message == WM_QUIT) {
                    SetRunning(false);
                }
            }
            else {
                // Run the update callback in a thread-safe way
                {
                    std::shared_lock lock(callbackMutex);
                    if (m_updateCallback) m_updateCallback();
                }

                if (std::chrono::duration_cast<std::chrono::minutes>(currentTime - lastSave).count() >= m_saveIntervalMinutes) {
                    Serialize("savegame.dat", m_events);
                    lastSave = currentTime;
                    std::cout << "Game auto-saved." << std::endl;
                }

                if (m_frameLimitingEnabled) {
                    LimitFrameRate(lastFrame);
                }
            }
        }
    }

    void AlmondShell::Run() {
        auto lastFrame = std::chrono::steady_clock::now();
        auto lastSave = std::chrono::steady_clock::now();

        while (m_running) {
            auto currentTime = std::chrono::steady_clock::now();

            {
                std::shared_lock lock(callbackMutex);
                if (m_updateCallback) m_updateCallback();
            }

            if (std::chrono::duration_cast<std::chrono::minutes>(currentTime - lastSave).count() >= m_saveIntervalMinutes) {
                Serialize("savegame.dat", m_events);
                lastSave = currentTime;
                std::cout << "Game auto-saved." << std::endl;
            }

            CaptureSnapshot();

            if (m_targetTime > 0.0f) {
                UpdatePlayback();
            }

            if (m_frameLimitingEnabled) {
                LimitFrameRate(lastFrame);
            }
        }
    }

    void AlmondShell::LimitFrameRate(std::chrono::steady_clock::time_point& lastFrame) {
        auto frameDuration = std::chrono::milliseconds(1000 / m_targetFPS);
        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastFrame = now - lastFrame;

        if (timeSinceLastFrame < frameDuration) {
            std::this_thread::sleep_for(frameDuration - timeSinceLastFrame);
        }

        lastFrame = std::chrono::steady_clock::now();
    }

    void AlmondShell::UpdateFPS() {
        m_frameCount++;
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - m_lastSecond).count();

        if (elapsedTime >= 1) {
            m_fps = static_cast<float>(m_frameCount) / elapsedTime;
            m_frameCount = 0;
            m_lastSecond = currentTime;

            std::cout << "Current FPS: " << std::fixed << std::setprecision(2) << m_fps << std::endl;
        }
    }

    void AlmondShell::Serialize(const std::string& filename, const std::vector<Event>& events) {
        try {
            std::ofstream ofs(filename, std::ios::binary);
            if (!ofs) throw std::ios_base::failure("Failed to open file for serialization");

            size_t size = events.size();
            ofs.write(reinterpret_cast<const char*>(&size), sizeof(size));
            for (const auto& event : events) {
                ofs.write(reinterpret_cast<const char*>(&event), sizeof(Event));
            }
        }
        catch (const std::ios_base::failure& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    void AlmondShell::Deserialize(const std::string& filename, std::vector<Event>& events) {
        try {
            std::ifstream ifs(filename, std::ios::binary);
            if (!ifs) throw std::ios_base::failure("Failed to open file for deserialization");

            size_t size;
            ifs.read(reinterpret_cast<char*>(&size), sizeof(size));
            events.resize(size);

            for (auto& event : events) {
                ifs.read(reinterpret_cast<char*>(&event), sizeof(Event));
            }
        }
        catch (const std::ios_base::failure& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    void AlmondShell::PrintOutFPS() {
        std::cout << "Average FPS: " << std::fixed << std::setprecision(2) << m_fps << std::endl;
    }

    bool AlmondShell::IsItRunning() const {
        return m_running;
    }

    void AlmondShell::SetRunning(bool running) {
        m_running = running;
    }

    void AlmondShell::PrintMessage(const std::string& text)
    {
        std::cout << text << std::endl;
    }

    void AlmondShell::SetFrameRate(unsigned int targetFPS) {
        m_targetFPS = targetFPS;
        m_frameLimitingEnabled = (m_targetFPS > 0);
    }

    // External entry points
    AlmondShell* CreateAlmondShell(size_t numThreads, bool running, Scene* scene, size_t maxBufferSize) {
        return new AlmondShell(numThreads, running, scene, maxBufferSize);
    }

    void Run(AlmondShell& core) {
        core.Run();
    }

    bool IsRunning(AlmondShell& core) {
        return core.IsItRunning();
    }

    void PrintFPS(AlmondShell& core) {
        core.PrintOutFPS();
    }
    /*
    void PrintMessage(AlmondShell& core, const std::string& text)
    {
        core.PrintToConsole(text);
    }*/

} // namespace almond

int GetMajor()
{
    return major;
}
int GetMinor()
{
    return minor;
}
int GetRevision()
{
    return revision;
}
static char version_string[32] = "";

extern "C" const char* GetEngineVersion()
{
    std::snprintf(version_string, sizeof(version_string), "%d.%d.%d", major, minor, revision);
    // std::cout << "Version String (in library): " << version_string << std::endl;
    return version_string;
}