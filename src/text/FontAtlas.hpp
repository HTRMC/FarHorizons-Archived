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
 */
class FontAtlas {
public:
    FontAtlas() = default;

    /**
     * Initialize a grid-based font atlas.
     * @param textureWidth Width of the font texture
     * @param textureHeight Height of the font texture
     * @param charsPerRow Number of characters per row in the texture
     * @param charsPerCol Number of characters per column in the texture
     * @param firstChar First character code in the atlas (usually 32 for space)
     */
    void initGrid(uint32_t textureWidth, uint32_t textureHeight,
                  uint32_t charsPerRow, uint32_t charsPerCol,
                  uint32_t firstChar = 32) {
        textureWidth_ = textureWidth;
        textureHeight_ = textureHeight;
        charsPerRow_ = charsPerRow;
        charsPerCol_ = charsPerCol;

        float charWidth = static_cast<float>(textureWidth) / charsPerRow;
        float charHeight = static_cast<float>(textureHeight) / charsPerCol;

        // UV padding to prevent texture bleeding (0.01 pixel inset)
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

            characters_[charCode] = info;
        }

        lineHeight_ = charHeight;
    }

    /**
     * Add or update a character in the atlas.
     */
    void addCharacter(uint32_t charCode, const CharInfo& info) {
        characters_[charCode] = info;
    }

    /**
     * Get character info for a specific character.
     */
    const CharInfo* getCharacter(uint32_t charCode) const {
        auto it = characters_.find(charCode);
        if (it != characters_.end()) {
            return &it->second;
        }
        // Return space character or first character as fallback
        auto fallback = characters_.find(32);
        return fallback != characters_.end() ? &fallback->second : nullptr;
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

    float getLineHeight() const { return lineHeight_; }
    void setLineHeight(float height) { lineHeight_ = height; }

    uint32_t getTextureWidth() const { return textureWidth_; }
    uint32_t getTextureHeight() const { return textureHeight_; }

private:
    std::unordered_map<uint32_t, CharInfo> characters_;
    uint32_t textureWidth_ = 0;
    uint32_t textureHeight_ = 0;
    uint32_t charsPerRow_ = 0;
    uint32_t charsPerCol_ = 0;
    float lineHeight_ = 16.0f;
};

} // namespace FarHorizon