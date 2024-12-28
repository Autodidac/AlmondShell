#include "SDL3Context.h"
#include "SDLSnake.h"
#include "SDLGameOfLife.h"
#include "SDLSandSim.h"

#include "Engine.h"

#ifdef ALMOND_USING_SDL
#include <algorithm> // for std::find_if

namespace almond {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_GLContext glContext;

    //SnakeGame::SnakeGame snakeGame;

    void initSDL3() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            throw std::runtime_error("Failed to initialize SDL: " + std::string(SDL_GetError()));
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        window = SDL_CreateWindow("Almond - SDL Example", 800, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
        if (!window) {
            SDL_Quit();
            throw std::runtime_error("Failed to create SDL window: " + std::string(SDL_GetError()));
        }

        renderer = SDL_CreateRenderer(window, NULL);
        if (!renderer) {
            SDL_DestroyWindow(window);
            SDL_Quit();
            throw std::runtime_error("Failed to create SDL renderer: " + std::string(SDL_GetError()));
        }

        glContext = SDL_GL_CreateContext(window);
        if (!glContext) {
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            throw std::runtime_error("Failed to create SDL OpenGL context: " + std::string(SDL_GetError()));
        }
    }

    bool processSDL3() {
        // Seed the random number generator
        srand(static_cast<unsigned int>(time(0)));

        // Create the cellular automaton, sand simulation, and snake game
        CellularAutomaton automaton(80, 60); // Grid size for Game of Life
        SandSimulation sandSim(160, 120);    // Grid size for sand simulation
        SnakeGame snake(40, 30);

        automaton.randomize();
        sandSim.randomize();

        bool running = true;
        bool isGameOfLife = true; // Start with Game of Life simulation
        bool isSnakeGame = false;  // Toggle between simulations

        // Variables for mouse input
        float mouseX = 0, mouseY = 0;
        bool isMousePressed = false;

        const Uint32 frameDelay = 200; // ~60 frames per second (1000ms / 60 ≈ 16ms)
        Uint32 lastTime = SDL_GetTicks();
        float deltaTime = 0.0f; // Time between updates

        while (running) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) {
                    running = false; // Exit when the window is closed
                }

                // Toggle between the simulations on key press
                if (e.type == SDL_EVENT_KEY_DOWN) {
                    switch (e.key.key) {
                    case SDLK_UP:
                        snake.updateDirection(SnakeGame::Direction::Up);
                        break;
                    case SDLK_DOWN:
                        snake.updateDirection(SnakeGame::Direction::Down);
                        break;
                    case SDLK_LEFT:
                        snake.updateDirection(SnakeGame::Direction::Left);
                        break;
                    case SDLK_RIGHT:
                        snake.updateDirection(SnakeGame::Direction::Right);
                        break;
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    }

                    if (e.key.key == SDLK_SPACE) {
                        isGameOfLife = !isGameOfLife; // Switch between Game of Life and sand simulation
                        isSnakeGame = false; // Ensure snake game is not active when switching
                    }
                    else if (e.key.key == SDLK_TAB) {
                        isSnakeGame = !isSnakeGame; // Toggle the snake game
                        isGameOfLife = false;      // Ensure Game of Life is not active when switching
                    }
                }

                // Handle mouse events
                if (e.type == SDL_EVENT_MOUSE_MOTION) {
                    SDL_GetMouseState(&mouseX, &mouseY);
                }
                if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        isMousePressed = true;
                    }
                }
                if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        isMousePressed = false;
                    }
                }
            }

            // Calculate the time elapsed
            Uint32 currentTime = SDL_GetTicks();
            deltaTime = (currentTime - lastTime) / 1000.0f; // Convert milliseconds to seconds
            lastTime = currentTime;

            // Update the chosen simulation based on deltaTime
            if (isMousePressed && !isGameOfLife && !isSnakeGame) {
                int gridX = static_cast<int>(mouseX / 5); // Calculate grid X position for sand simulation
                int gridY = static_cast<int>(mouseY / 5); // Calculate grid Y position
                sandSim.addSandAt(gridX, gridY);
            }

            if (isGameOfLife) {
                automaton.update();
            }
            else if (isSnakeGame) {
                snake.update(deltaTime);
            }
            else {
                sandSim.update();
            }

            // Clear the renderer and render the chosen simulation
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black background
            SDL_RenderClear(renderer);

            if (isGameOfLife) {
                automaton.render(renderer, 10); // Cell size for Game of Life
            }
            else if (isSnakeGame) {
                snake.render(renderer, 20); // Cell size for snake game
            }
            else {
                sandSim.render(renderer, 5); // Cell size for sand simulation
            }

            SDL_RenderPresent(renderer);
        }
        return false;
    }

    void cleanupSDL3() {
        if (renderer) {
            SDL_DestroyRenderer(renderer);
            renderer = nullptr;
        }
        if (window) {
            SDL_DestroyWindow(window);
            window = nullptr;
        }
        SDL_Quit();
    }

    inline ContextFuncs createContextFuncs(const std::string& type) {
        if (type == "SDL3") {
            return { initSDL3, processSDL3, cleanupSDL3 };
        }
        // Handle other types or throw an error if not found
        throw std::invalid_argument("Unsupported context type: " + type);
    }
}
#endif
