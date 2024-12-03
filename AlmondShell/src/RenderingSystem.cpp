#include "RenderingSystem.h"
#include "framework.h"

#include <stdexcept>

namespace almond {

    RenderingSystem::RenderingSystem(const std::string& text, float x, float y, float width, float height, unsigned int color, void* texture) {
        // Initialization code...
    }

    RenderingSystem::~RenderingSystem() {
        // Cleanup code...
    }

    // Define the template function inline in the .cpp file
 /*  template<typename T>
    inline void CreateContextRenderer(const std::string& text, float x, float y, float width, float height, unsigned int color, void* texture) requires RenderContextConcept<T> {
            // Create and move unique_ptr into the vector
        m_context = std::make_unique<T>(text, x, y, width, height, color, texture);
            contexts.push_back(std::move(m_context));
        }
*/
    void RenderingSystem::DestroyContextRenderer(size_t contextID) {
        if (contextID >= contexts.size()) {
            throw std::out_of_range("Invalid context ID");
        }
        contexts.erase(contexts.begin() + contextID);
    }

    void RenderingSystem::RenderAll() {
        for (size_t i = 0; i < contexts.size(); ++i) {
            RenderContextByID(i);
        }
    }

    void RenderingSystem::RenderContextByID(size_t contextID) {
        if (contextID >= contexts.size()) {
            throw std::out_of_range("Invalid context ID");
        }

        RenderContext& context = *contexts[contextID];
        context.BeginFrame();
        DrawRect(context, 50.0f, 50.0f, 200.0f, 100.0f, 0xFF00FF00);
        DrawAlmondText(context, "Rendering System!", 100.0f, 100.0f, 800.0f, 600.0f, 0xFFFFFFFF);
        context.EndFrame();
    }

    void RenderingSystem::DrawRect(RenderContext& context, float x, float y, float width, float height, unsigned int color) {
        context.DrawRectangle(x, y, width, height, color);
    }

    void RenderingSystem::DrawAlmondText(RenderContext& context, const std::string& text, float x, float y, float width, float height, unsigned int color) {
        context.DrawAlmondText(text, x, y, width, height, color);
    }

    void RenderingSystem::DrawImage(RenderContext& context, float x, float y, float width, float height, void* texture) {
        context.DrawImage(x, y, width, height, texture);
    }

} // namespace almond
