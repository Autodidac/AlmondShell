#pragma once

#include "MovementEvent.h"

#include <vector>
#include <memory>
#include <map>

namespace almond {
   /* struct Event {
        std::string type;
        std::map<std::string, std::string> data;
    };*/

    enum class EventType {
        MouseButtonClick,
        MouseMove,
        KeyPress,
        TextInput
    };

    struct Event {
        EventType type;
        std::map<std::string, std::string> data;
        float x, y;  // Mouse position
        int key;     // Key press code
        char text[2]; // Text input (character)
    };

    class EventManager {
    public:

        // Add a movement event as a unique pointer
        void addMovementEvent(std::unique_ptr<MovementEvent> event) {
            movementEvents.push_back(std::move(event));
        }

        // Return a vector of movement events as unique pointers and clear the internal storage
        std::vector<std::unique_ptr<MovementEvent>> getMovementEvents() {
            std::vector<std::unique_ptr<MovementEvent>> eventsCopy;
            eventsCopy.swap(movementEvents); // Efficiently swap the contents
            return eventsCopy; // Return the unique pointers
        }

        // Clear the movement events (if needed, although it's already cleared after getMovementEvents)
        void clearMovementEvents() {
            movementEvents.clear(); // Clear the stored events
        }

    private:
        std::vector<std::unique_ptr<MovementEvent>> movementEvents; // Store movement events as unique pointers
    };
} // namespace almond
