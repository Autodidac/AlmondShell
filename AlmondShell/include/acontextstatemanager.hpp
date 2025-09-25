/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   This file is part of the Almond Project.                 *
 *   AlmondShell - Modular C++ Framework                      *
 *                                                            *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
 *                                                            *
 *   Provided "AS IS", without warranty of any kind.          *
 *   Use permitted for Non-Commercial Purposes ONLY,          *
 *   without prior commercial licensing agreement.            *
 *                                                            *
 *   Redistribution Allowed with This Notice and              *
 *   LICENSE file. No obligation to disclose modifications.   *
 *                                                            *
 *   See LICENSE file for full terms.                         *
 *                                                            *
 **************************************************************/
#pragma once

#include "aplatform.hpp"      // must always come first
#include "acontextstate.hpp"  // ContextState definition

#include <memory>
#include <stdexcept>

namespace almondnamespace
{
    class ContextManager
    {
    public:
        // Set the active context
        static void setContext(std::shared_ptr<state::ContextState> ctx) noexcept
        {
            instance() = std::move(ctx);
        }

        // Get the active context (shared_ptr copy)
        static const std::shared_ptr<state::ContextState> getContext()
        {
            auto& inst = instance();
            if (!inst)
                throw std::runtime_error("No active ContextState set");
            return inst; // returns shared_ptr copy
        }

        // Get the active context by reference (unsafe if reset)
        static state::ContextState& getContextRef()
        {
            auto& inst = instance();
            if (!inst)
                throw std::runtime_error("No active ContextState set");
            return *inst;
        }

        // Reset the active context
        static void resetContext() noexcept
        {
            instance().reset();
        }

        // Query if a context is set
        static bool hasContext() noexcept
        {
            return static_cast<bool>(instance());
        }

    private:
        // Singleton shared_ptr instance
        static std::shared_ptr<state::ContextState>& instance()
        {
            static std::shared_ptr<state::ContextState> s_instance;
            return s_instance;
        }
    };
}
