#pragma once

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace FarHorizon {

/**
 * Character information for a font atlas.
 */
struct CharInfo {
    glm::vec2 uvMin;      // Top-left UV coordinate
    glm::vec2 uvMax;      // Bottom-right UV coordinate
    glm::vec2 size;       // Character size in pixels
    glm::vec2 offset;     // Rendering offset
    float advance;        // Horizontal advance for next character
};

/**
 * Font atlas that manages character UV coordinates and metrics.
 * Supports bitmap fonts similar to Minecraft's font system.
 */
class FontAtlas {
public:
    FontAtlas() = default;

    /**
     * Initialize a grid-based font atlas (like Minecraft's ASCII font).
     * @param textureWidth Width of the font texture
     * @param textureHeight Height of the font texture
     * @param charsPerRow Number of characters per row in the texture
     * @param charsPerCol Number of characters per column in the texture
     * @param firstChar First character code in the atlas (usually 32 for space)
     */
    void initGrid(uint32_t textureWidth, uint32_t textureHeight,
                  uint32_t charsPerRow, uint32_t charsPerCol,
                  uint32_t firstChar = 32) {
        m_textureWidth = textureWidth;
        m_textureHeight = textureHeight;
        m_charsPerRow = charsPerRow;
        m_charsPerCol = charsPerCol;

        float charWidth = static_cast<float>(textureWidth) / charsPerRow;
        float charHeight = static_cast<float>(textureHeight) / charsPerCol;

        // UV padding to prevent texture bleeding (Minecraft-style: 0.01 pixel inset)
        const float uvPadding = 0.01f;

        // Generate character info for ASCII printable characters
        for (uint32_t i = 0; i < charsPerRow * charsPerCol; ++i) {
            uint32_t charCode = firstChar + i;

            uint32_t col = i % charsPerRow;
            uint32_t row = i / charsPerRow;

            CharInfo info;
            // Apply UV inset padding to prevent bleeding between glyphs
            info.uvMin = glm::vec2(
                (static_cast<float>(col * charWidth) + uvPadding) / textureWidth,
                (static_cast<float>(row * charHeight) + uvPadding) / textureHeight
            );
            info.uvMax = glm::vec2(
                (static_cast<float>((col + 1) * charWidth) - uvPadding) / textureWidth,
                (static_cast<float>((row + 1) * charHeight) - uvPadding) / textureHeight
            );
            info.size = glm::vec2(charWidth, charHeight);
            info.offset = glm::vec2(0.0f, 0.0f);
            info.advance = charWidth;

            m_characters[charCode] = info;
        }

        m_lineHeight = charHeight;
    }

    /**
     * Add or update a character in the atlas.
     */
    void addCharacter(uint32_t charCode, const CharInfo& info) {
        m_characters[charCode] = info;
    }

    /**
     * Get character info for a specific character.
     */
    const CharInfo* getCharacter(uint32_t charCode) const {
        auto it = m_characters.find(charCode);
        if (it != m_characters.end()) {
            return &it->second;
        }
        // Return space character or first character as fallback
        auto fallback = m_characters.find(32);
        return fallback != m_characters.end() ? &fallback->second : nullptr;
    }

    /**
     * Calculate the width of a string in pixels.
     */
    float calculateWidth(const std::string& text) const {
        float width = 0.0f;
        for (char c : text) {
            auto* info = getCharacter(static_cast<uint32_t>(c));
            if (info) {
                width += info->advance;
            }
        }
        return width;
    }

    float getLineHeight() const { return m_lineHeight; }
    void setLineHeight(float height) { m_lineHeight = height; }

    uint32_t getTextureWidth() const { return m_textureWidth; }
    uint32_t getTextureHeight() const { return m_textureHeight; }

private:
    std::unordered_map<uint32_t, CharInfo> m_characters;
    uint32_t m_textureWidth = 0;
    uint32_t m_textureHeight = 0;
    uint32_t m_charsPerRow = 0;
    uint32_t m_charsPerCol = 0;
    float m_lineHeight = 16.0f;
};

} // namespace FarHorizon