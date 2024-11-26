#pragma once

#include "ComponentManager.h"
#include "ThreadPool.h"

namespace almond {
    class PhysicsSystem {
    public:
        PhysicsSystem(ComponentManager& componentManager, ThreadPool& jobSystem);
        void update(float deltaTime);

    private:
        ComponentManager& componentManager;
        ThreadPool& jobSystem;
    };

    inline PhysicsSystem::PhysicsSystem(ComponentManager& componentManager, ThreadPool& jobSystem)
        : componentManager(componentManager), jobSystem(jobSystem) {}

    inline void PhysicsSystem::update(float deltaTime) {
        // Update physics components
        // Implement physics calculations and updates
    }
} // namespace almond
