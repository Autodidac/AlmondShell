// acommandqueue.hpp
#pragma once

#include <functional>
#include <mutex>
#include <queue>

namespace almondnamespace::core {

    struct CommandQueue {
        using RenderCommand = std::function<void()>;

        std::queue<RenderCommand>& get_queue() { return commands; }
        std::mutex& get_mutex() { return mutex; }

        void enqueue(RenderCommand cmd) {
            std::scoped_lock lock(mutex);
            commands.push(std::move(cmd));
        }

        bool drain() {
            std::queue<RenderCommand> localCommands;
            {
                std::scoped_lock lock(mutex);
                if (commands.empty()) return false;
                localCommands.swap(commands);
            }

            while (!localCommands.empty()) {
                auto cmd = std::move(localCommands.front());
                localCommands.pop();
                if (cmd) cmd();
            }
            return true;
        }

    private:
        std::mutex mutex;
        std::queue<RenderCommand> commands;
    };

} // namespace almondshell::core
