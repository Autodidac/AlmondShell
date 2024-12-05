#include "ContextFactory.h"

#ifdef ALMOND_USING_SFML

#include <SFML/Window.hpp>
//#include <event.xsd>

namespace almond {

    WindowContext CreateSFMLWindowContext() {
        sf::Window* window = nullptr;

        return {
            .createWindow = [&](const std::string& title, int width, int height) -> void* {
                window = new sf::Window(sf::VideoMode(width, height), title);
                return window;
            },
            .pollEvents = [&]() {
                sf::Event event;
                while (window->pollEvent(event)) {
                    if (event.type == sf::Event::Closed) {
                        window->close();
                    }
                }
            },
            .shouldClose = [&]() -> bool {
                return !window->isOpen();
            },
            .getNativeHandle = [&]() -> void* {
                return static_cast<void*>(window);
            }
        };
    }

} // namespace almond
#endif