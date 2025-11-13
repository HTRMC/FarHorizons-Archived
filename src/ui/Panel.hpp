#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace FarHorizon {

/**
 * Simple UI panel for rendering colored rectangles.
 * Used for backgrounds, overlays, and UI containers.
 */
struct PanelVertex {
    glm::vec2 position;  // Screen position in NDC
    glm::vec4 color;     // RGBA color

    static std::vector<VkVertexInputBindingDescription> getBindingDescriptions() {
        std::vector<VkVertexInputBindingDescription> bindings(1);
        bindings[0].binding = 0;
        bindings[0].stride = sizeof(PanelVertex);
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindings;
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributes(2);

        // Position
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[0].offset = offsetof(PanelVertex, position);

        // Color
        attributes[1].binding = 0;
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes[1].offset = offsetof(PanelVertex, color);

        return attributes;
    }
};

class Panel {
public:
    Panel(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
        : position_(position)
        , size_(size)
        , color_(color) {}

    // Generate vertices for a quad (two triangles)
    std::vector<PanelVertex> generateVertices(uint32_t screenWidth, uint32_t screenHeight) const {
        // Convert screen coordinates to NDC (-1 to 1)
        float x1 = (position_.x / screenWidth) * 2.0f - 1.0f;
        float y1 = (position_.y / screenHeight) * 2.0f - 1.0f;
        float x2 = ((position_.x + size_.x) / screenWidth) * 2.0f - 1.0f;
        float y2 = ((position_.y + size_.y) / screenHeight) * 2.0f - 1.0f;

        // Create quad as two triangles (6 vertices)
        return {
            // Triangle 1
            {{x1, y1}, color_},  // Top-left
            {{x2, y1}, color_},  // Top-right
            {{x1, y2}, color_},  // Bottom-left

            // Triangle 2
            {{x2, y1}, color_},  // Top-right
            {{x2, y2}, color_},  // Bottom-right
            {{x1, y2}, color_}   // Bottom-left
        };
    }

    // Getters/Setters
    void setPosition(const glm::vec2& pos) { position_ = pos; }
    void setSize(const glm::vec2& size) { size_ = size; }
    void setColor(const glm::vec4& color) { color_ = color; }

    const glm::vec2& getPosition() const { return position_; }
    const glm::vec2& getSize() const { return size_; }
    const glm::vec4& getColor() const { return color_; }

    // Helper to create semi-transparent overlay
    static Panel createOverlay(uint32_t screenWidth, uint32_t screenHeight, float alpha = 0.5f) {
        return Panel(
            glm::vec2(0, 0),
            glm::vec2(screenWidth, screenHeight),
            glm::vec4(0.0f, 0.0f, 0.0f, alpha)  // Black with alpha
        );
    }

    // Helper to create blur-like frosted glass overlay (transparent - blur applied via shader)
    static Panel createBlurOverlay(uint32_t screenWidth, uint32_t screenHeight) {
        return Panel(
            glm::vec2(0, 0),
            glm::vec2(screenWidth, screenHeight),
            glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)  // Fully transparent - blur is applied via post-processing
        );
    }

private:
    glm::vec2 position_;
    glm::vec2 size_;
    glm::vec4 color_;
};

} // namespace FarHorizon