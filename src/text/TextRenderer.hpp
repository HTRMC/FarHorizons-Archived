#pragma once

#include "Text.hpp"
#include "FontManager.hpp"
#include "../renderer/memory/Buffer.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace VoxelEngine {

/**
 * Vertex structure for text rendering.
 */
struct TextVertex {
    glm::vec2 position;    // Screen position
    glm::vec2 texCoord;    // UV coordinates
    glm::vec4 color;       // Text color
    uint32_t textureIndex; // Font texture index
};

/**
 * Renders styled text to screen using font atlases.
 */
class TextRenderer {
public:
    TextRenderer() = default;

    void init(FontManager* fontManager) {
        m_fontManager = fontManager;
    }

    /**
     * Generate vertices for rendering text at a specific position.
     * @param text The text to render
     * @param position Screen position (in pixels, top-left is 0,0)
     * @param scale Scale factor for the text
     * @param screenWidth Screen width for coordinate normalization
     * @param screenHeight Screen height for coordinate normalization
     * @return Vector of vertices ready for rendering
     */
    std::vector<TextVertex> generateVertices(const Text& text,
                                             glm::vec2 position,
                                             float scale = 1.0f,
                                             uint32_t screenWidth = 1600,
                                             uint32_t screenHeight = 900) {
        std::vector<TextVertex> vertices;

        if (!m_fontManager) {
            spdlog::error("TextRenderer not initialized with font manager");
            return vertices;
        }

        glm::vec2 cursor = position;

        for (const auto& segment : text.getSegments()) {
            const Style& style = segment.style;
            FontAtlas* atlas = m_fontManager->getFont(style.getFont());
            uint32_t textureIndex = m_fontManager->getFontTexture(style.getFont());

            if (!atlas) {
                spdlog::warn("Font '{}' not found, skipping segment", style.getFont());
                continue;
            }

            // Render shadow first if enabled
            if (style.hasShadow()) {
                glm::vec2 shadowOffset = glm::vec2(1.0f, 1.0f) * scale;
                generateSegmentVertices(vertices, segment.content, cursor + shadowOffset,
                                       scale, atlas, textureIndex,
                                       style.getShadowColor(), screenWidth, screenHeight);
            }

            // Render main text
            cursor.x += generateSegmentVertices(vertices, segment.content, cursor,
                                               scale, atlas, textureIndex,
                                               style.getColor(), screenWidth, screenHeight);
        }

        return vertices;
    }

    /**
     * Calculate the width of text in pixels.
     */
    float calculateTextWidth(const Text& text, float scale = 1.0f) {
        if (!m_fontManager) return 0.0f;

        float width = 0.0f;
        for (const auto& segment : text.getSegments()) {
            FontAtlas* atlas = m_fontManager->getFont(segment.style.getFont());
            if (atlas) {
                width += atlas->calculateWidth(segment.content) * scale;
            }
        }
        return width;
    }

    /**
     * Calculate the height of text in pixels.
     */
    float calculateTextHeight(const Text& text, float scale = 1.0f) {
        if (!m_fontManager) return 0.0f;

        // Use the first segment's font for height
        if (text.getSegments().empty()) return 0.0f;

        FontAtlas* atlas = m_fontManager->getFont(text.getSegments()[0].style.getFont());
        if (atlas) {
            return atlas->getLineHeight() * scale;
        }
        return 0.0f;
    }

private:
    /**
     * Generate vertices for a single text segment.
     * Returns the horizontal advance (width) of the rendered text.
     */
    float generateSegmentVertices(std::vector<TextVertex>& vertices,
                                  const std::string& content,
                                  glm::vec2 position,
                                  float scale,
                                  FontAtlas* atlas,
                                  uint32_t textureIndex,
                                  const glm::vec4& color,
                                  uint32_t screenWidth,
                                  uint32_t screenHeight) {
        float startX = position.x;

        for (char c : content) {
            const CharInfo* charInfo = atlas->getCharacter(static_cast<uint32_t>(c));
            if (!charInfo) continue;

            // Calculate character quad positions
            glm::vec2 charSize = charInfo->size * scale;
            glm::vec2 charOffset = charInfo->offset * scale;

            // Convert pixel coordinates to normalized device coordinates (-1 to 1)
            glm::vec2 pos0 = pixelToNDC(position + charOffset, screenWidth, screenHeight);
            glm::vec2 pos1 = pixelToNDC(position + charOffset + charSize, screenWidth, screenHeight);

            // Create quad (two triangles)
            // Triangle 1: top-left, bottom-left, top-right
            vertices.push_back({pos0, charInfo->uvMin, color, textureIndex});
            vertices.push_back({{pos0.x, pos1.y}, {charInfo->uvMin.x, charInfo->uvMax.y}, color, textureIndex});
            vertices.push_back({{pos1.x, pos0.y}, {charInfo->uvMax.x, charInfo->uvMin.y}, color, textureIndex});

            // Triangle 2: top-right, bottom-left, bottom-right
            vertices.push_back({{pos1.x, pos0.y}, {charInfo->uvMax.x, charInfo->uvMin.y}, color, textureIndex});
            vertices.push_back({{pos0.x, pos1.y}, {charInfo->uvMin.x, charInfo->uvMax.y}, color, textureIndex});
            vertices.push_back({pos1, charInfo->uvMax, color, textureIndex});

            // Advance cursor
            position.x += charInfo->advance * scale;
        }

        return position.x - startX;
    }

    /**
     * Convert pixel coordinates to normalized device coordinates.
     * Top-left is (-1, -1), bottom-right is (1, 1).
     */
    glm::vec2 pixelToNDC(glm::vec2 pixel, uint32_t screenWidth, uint32_t screenHeight) {
        return glm::vec2(
            (pixel.x / screenWidth) * 2.0f - 1.0f,
            (pixel.y / screenHeight) * 2.0f - 1.0f
        );
    }

    FontManager* m_fontManager = nullptr;
};

} // namespace VoxelEngine