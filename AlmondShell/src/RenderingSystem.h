#pragma once

#include "ComponentManager.h"
#include "ThreadPool.h"

namespace almond {
class RenderingSystem {
public:
    RenderingSystem(ComponentManager& componentManager, ThreadPool& jobSystem);
    void render();

private:
    ComponentManager& componentManager;
    ThreadPool& jobSystem;

    // ux/ui
    void DrawRect(float x, float y, float width, float height, unsigned int color);
    void DrawText(float x, float y, const std::string& text, unsigned int color);
    void DrawImage(float x, float y, float width, float height, void* texture);
    void BeginDraw();
    void EndDraw();

};

inline RenderingSystem::RenderingSystem(ComponentManager& componentManager, ThreadPool& jobSystem)
    : componentManager(componentManager), jobSystem(jobSystem) {}

inline void RenderingSystem::render() {
    // Render graphics
    // Implement rendering logic here
}



} // namespace almond
