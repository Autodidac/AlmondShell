#include "Engine.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <thread>
#include <filesystem>
#include <stdexcept>

namespace almond {
    //class RenderingSystem;
    // Mutex for callback thread safety
    std::shared_mutex m_callbackMutex;
    std::function<void()> m_updateCallback;

    // Register a callback for updates
    void RegisterAlmondCallback(const std::function<void()> callback) {
        std::unique_lock lock(m_callbackMutex);
        m_updateCallback = callback;
    }

    // Constructor and Destructor
    AlmondShell::AlmondShell(size_t numThreads, bool running, almond::Scene* scene, size_t maxBufferSize)
        : m_jobSystem(numThreads), m_running(running),
        m_scene(scene ? std::make_unique<almond::Scene>(std::move(*scene)) : nullptr),
        m_maxBufferSize(maxBufferSize), m_targetTime(0.0f), m_targetFPS(120),
        m_frameLimitingEnabled(false), m_saveIntervalMinutes(1),
        m_frameCount(0), m_fps(0.0f) {
        // Create renderer instance      
       // m_renderer = std::make_unique<RenderingSystem>();
        InitializeRenderer("Example Almond Window Title", 5.0f, 5.0f, 800, 600, 0xFFFFFFFF, nullptr);
    }

    // Destructor
    AlmondShell::~AlmondShell() = default;

    // Snapshot management
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
        if (m_recentStates.empty()) return;

        try {
            std::ofstream ofs("savegame.dat", std::ios::binary | std::ios::app);
            if (!ofs.is_open()) {
                throw std::ios_base::failure("Failed to open save file for writing.");
            }

            const SceneSnapshot& snapshot = m_recentStates.back();
            ofs.write(reinterpret_cast<const char*>(&snapshot), sizeof(SceneSnapshot));
            if (!ofs) {
                throw std::ios_base::failure("Error occurred while writing to save file.");
            }
        }
        catch (const std::ios_base::failure& e) {
            std::cerr << "SaveBinaryState Error: " << e.what() << std::endl;
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

    // Update FPS and frame limiting
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

    void AlmondShell::LimitFrameRate(std::chrono::steady_clock::time_point& lastFrame) {
        auto frameDuration = std::chrono::milliseconds(1000 / m_targetFPS);
        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastFrame = now - lastFrame;

        if (timeSinceLastFrame < frameDuration) {
            std::this_thread::sleep_for(frameDuration - timeSinceLastFrame);
        }
        lastFrame = std::chrono::steady_clock::now();
    }

    void AlmondShell::InitializeRenderer(const std::string& title, float x, float y, float width, float height, unsigned int color, void* texture) {
        // Ensure m_renderer is created only once
        if (!m_renderer) {
            std::cout << "Creating a new RenderingSystem instance.\n"; // Logging
            // Pass componentManager and jobSystem when constructing RenderingSystem
            m_renderer = std::make_unique<RenderingSystem>(title, x, y, width, height, color, texture);
        }

        // Initialize the context renderer, but only if it hasn't been set up yet
        if (!m_renderer->IsInitialized()) {
            try {
                std::cout << "Initializing context renderer with title: " << title << ", width: " << width << ", height: " << height << "\n"; // Logging
                m_renderer->CreateContextRenderer<RenderContext>(title, x, y, width, height, color, texture);
                std::cout << "Renderer initialized successfully.\n"; // Logging
            }
            catch (const std::exception& e) {
                std::cerr << "Error initializing the renderer: " << e.what() << '\n'; // Error handling
            }
        }
        else {
            std::cout << "Renderer is already initialized.\n"; // Logging
        }
    }

    void AlmondShell::RenderFrame() {
        if (m_renderer) {
            try {
                std::cout << "Rendering frame...\n"; // Logging

                // Clear the renderer to prepare for the new frame
                //m_renderer ->Clear();

                // Check if the scene exists and render it
                if (m_scene) {
                    std::cout << "Rendering scene...\n"; // Logging
                    //m_scene->Render(*m_renderer);
                }
                else {
                    std::cout << "No scene available to render.\n"; // Logging
                }

                // Render all items managed by the renderer
                m_renderer->RenderAll();

                std::cout << "Frame rendered successfully.\n"; // Logging
            }
            catch (const std::exception& e) {
                std::cerr << "Error during frame rendering: " << e.what() << '\n'; // Error handling
            }
        }
        else {
            std::cerr << "Renderer is not initialized. Unable to render frame.\n"; // Error handling
        }
    }

    // Win32-specific functionality
    void AlmondShell::RunWin32Desktop(MSG msg, HACCEL hAccelTable) {
        auto now = std::chrono::steady_clock::now();
        auto lastFrame = now;
        auto lastSave = now;

        while (m_running) {
            auto currentTime = std::chrono::steady_clock::now();
            UpdateFPS();

            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
                if (msg.message == WM_QUIT) {
                    m_running = false;
                }
            }
            else {
                std::shared_lock lock(m_callbackMutex);
                if (m_updateCallback) m_updateCallback();

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

    // Main run loop
    void AlmondShell::Run() {
        auto now = std::chrono::steady_clock::now();
        auto lastFrame = now;
        auto lastSave = now;

        EventSystem eventSystem;
        UIManager uiManager;

        UIButton* button = new UIButton(50, 50, 200, 50, "Click Me!");
        button->SetOnClick([]() { std::cout << "Button clicked!\n"; });
        uiManager.AddButton(button);

        //load plugins
        std::cout << "Loading any available plugins...\n";
        almond::plugin::PluginManager manager("plugin_manager.log");

        const std::filesystem::path pluginDirectory = ".\\mods";
        if (std::filesystem::exists(pluginDirectory) && std::filesystem::is_directory(pluginDirectory)) {
            for (const auto& entry : std::filesystem::directory_iterator(pluginDirectory)) {
                if (entry.path().extension() == ".dll" || entry.path().extension() == ".so") {
                    manager.LoadPlugin(entry.path());
                }
            }
        }

        // Initialize the renderer
        //InitializeRenderer("renderer initialized!",800,600);

        // headless main loop
        while (m_running) {
            auto currentTime = std::chrono::steady_clock::now();

            // Update events
            uiManager.Update(eventSystem);

            RenderFrame(); // Render the frame



            std::shared_lock lock(m_callbackMutex);
            if (m_updateCallback) m_updateCallback();

            // Perform a sequential scene buffer autosave
            if (std::chrono::duration_cast<std::chrono::minutes>(currentTime - lastSave).count() >= m_saveIntervalMinutes) {
                Serialize("savegame.dat", m_events);
                lastSave = currentTime;
                std::cout << "Game auto-saved." << std::endl;
            }

            // Immediately Capture Current Scene Data Snapshot - ordering may need to be changed
            CaptureSnapshot();

            if (m_targetTime > 0.0f) {
                UpdatePlayback();
            }

            if (m_frameLimitingEnabled) {
                LimitFrameRate(lastFrame);
            }
        }
    }

    // Serialize and deserialize
    void AlmondShell::Serialize(const std::string& filename, const std::vector<Event>& events) {
        try {
            std::ofstream ofs(filename, std::ios::binary);
            if (!ofs) throw std::ios_base::failure("Failed to open file for serialization");

            size_t size = events.size();
            ofs.write(reinterpret_cast<const char*>(&size), sizeof(size));
            ofs.write(reinterpret_cast<const char*>(events.data()), sizeof(Event) * size);
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
            ifs.read(reinterpret_cast<char*>(events.data()), sizeof(Event) * size);
        }
        catch (const std::ios_base::failure& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    void AlmondShell::PrintOutFPS() { std::cout << "Average FPS: " << std::fixed << std::setprecision(2) << m_fps << std::endl; }
    bool AlmondShell::IsItRunning() const { return m_running; }
    void AlmondShell::SetRunning(bool running) { m_running = running; }
    void AlmondShell::PrintMessage(const std::string& text) { std::cout << text << std::endl; }

    void AlmondShell::SetFrameRate(unsigned int targetFPS) {
        m_targetFPS = targetFPS;
        m_frameLimitingEnabled = (m_targetFPS > 0);
    }

    // External entry points
    AlmondShell* CreateAlmondShell(size_t numThreads, bool running, Scene* scene, size_t maxBufferSize) {
        return new AlmondShell(numThreads, running, scene, maxBufferSize);
    }

    void Run(AlmondShell& core) { core.Run(); }
    bool IsRunning(AlmondShell& core) { return core.IsItRunning(); }
    void PrintFPS(AlmondShell& core) { core.PrintOutFPS(); }
    /*
    void PrintMessage(AlmondShell& core, const std::string& text)
    {
        core.PrintToConsole(text);
    }*/

} // namespace almond
