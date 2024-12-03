#pragma once

//#include "ComponentManager.h"
//#include "ThreadPool.h"
#include "Context_Concept.h"
#include "Context_Renderer.h"
#include "Exports_DLL.h"

#include <memory>
#include <string>
#include <vector>

namespace almond {

    class RenderContext;

    class RenderingSystem {
    public:
        RenderingSystem(const std::string& text, float x, float y, float width, float height, unsigned int color, void* texture);
        ~RenderingSystem();

        bool IsInitialized() const {
            return m_isInitialized;  // This should be a member variable set to true after initialization
        }

        // Managing rendering contexts
        template<typename T>
        void CreateContextRenderer(const std::string& text, float x, float y, float width, float height, unsigned int color, void* texture) requires RenderContextConcept<T> {
            // Create and move unique_ptr into the vector
            auto context = std::make_unique<T>(text, x, y, width, height, color, texture);
            contexts.push_back(std::move(context));
        }

        void RenderContextByID(size_t contextID);
        void RenderAll();

        void DestroyContextRenderer(size_t contextID);
        std::vector<std::unique_ptr<RenderContext>> contexts;

    private:
        bool m_isInitialized = false;
        //std::unique_ptr<RenderContext> m_context;

        void DrawRect(RenderContext& context, float x, float y, float width, float height, unsigned int color);
        void DrawAlmondText(RenderContext& context, const std::string& text, float x, float y, float width, float height, unsigned int color);
        void DrawImage(RenderContext& context, float x, float y, float width, float height, void* texture);
    };
}