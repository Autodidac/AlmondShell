// asceneerialize.hpp
#pragma once

#include "ascene.hpp"   // almondnamespace::Scene
#include "ascenesnapshot.hpp" // almondnamespace::SceneSnapshot
#include <ostream>
#include <istream>

namespace almondnamespace {

    // Write one snapshot to binary stream
    inline void serializeSnapshot(const SceneSnapshot& snap, std::ostream& os) {
        float ts = snap.timeStamp;
        os.write(reinterpret_cast<const char*>(&ts), sizeof(float));

        // assuming Scene has a getter for entities
        const auto& ents = snap.currentState->getEntities();
        int count = static_cast<int>(ents.size());
        os.write(reinterpret_cast<const char*>(&count), sizeof(int));

        for (const auto& e : ents) {
            int id = e->getId();
            float x = e->getX();
            float y = e->getY();
            os.write(reinterpret_cast<const char*>(&id), sizeof(int));
            os.write(reinterpret_cast<const char*>(&x), sizeof(float));
            os.write(reinterpret_cast<const char*>(&y), sizeof(float));
        }
    }

    // Read one snapshot from binary stream
    inline SceneSnapshot deserializeSnapshot(std::istream& is) {
        float ts;
        is.read(reinterpret_cast<char*>(&ts), sizeof(float));

        int count;
        is.read(reinterpret_cast<char*>(&count), sizeof(int));

        auto scene = std::make_unique<Scene>();
        scene->clearEntities();

        for (int i = 0; i < count; ++i) {
            int id;
            float x, y;
            is.read(reinterpret_cast<char*>(&id), sizeof(int));
            is.read(reinterpret_cast<char*>(&x), sizeof(float));
            is.read(reinterpret_cast<char*>(&y), sizeof(float));

            auto ent = std::make_unique<ecs::Entity>(id, x, y);
            scene->addEntity(std::move(ent));
        }

        return SceneSnapshot(ts, std::move(scene));
    }

} // namespace almondnamespace
