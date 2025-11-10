#pragma once

#include "FontAtlas.hpp"
#include "../renderer/texture/BindlessTextureManager.hpp"
#include "../renderer/texture/TextureLoader.hpp"
#include <unordered_map>
#include <memory>
#include <spdlog/spdlog.h>

namespace FarHorizon {

/**
 * Manages multiple font atlases and their textures.
 * Similar to Minecraft's FontManager.
 */
class FontManager {
public:
    FontManager() = default;

    /**
     * Initialize the font manager with a texture manager.
     */
    void init(BindlessTextureManager* textureManager) {
        m_textureManager = textureManager;
    }

    /**
     * Load a simple grid-based font (like Minecraft's ASCII font).
     * @param fontName Name to identify this font
     * @param texturePath Path to the font texture
     * @param uploadCmd Command buffer for texture upload
     * @param textureWidth Width of the font texture
     * @param textureHeight Height of the font texture
     * @param charsPerRow Number of characters per row
     * @param charsPerCol Number of characters per column
     * @param firstChar First character code (usually 32 for space)
     */
    uint32_t loadGridFont(const std::string& fontName,
                          const std::string& texturePath,
                          VkCommandBuffer uploadCmd,
                          uint32_t textureWidth = 128,
                          uint32_t textureHeight = 128,
                          uint32_t charsPerRow = 16,
                          uint32_t charsPerCol = 16,
                          uint32_t firstChar = 0) {
        if (!m_textureManager) {
            spdlog::error("FontManager not initialized with texture manager");
            return 0;
        }

        // Load the texture pixel data for width calculation
        TextureData textureData;
        try {
            textureData = TextureLoader::loadPNG(texturePath);
        } catch (const std::exception& e) {
            spdlog::error("Failed to load font texture for width calculation: {}", e.what());
            return 0;
        }

        // Load the texture into GPU
        uint32_t textureIndex = m_textureManager->loadTexture(texturePath, uploadCmd, false);
        if (textureIndex == 0) {
            spdlog::error("Failed to load font texture: {}", texturePath);
            return 0;
        }

        // Calculate character cell size
        uint32_t charWidth = textureWidth / charsPerRow;
        uint32_t charHeight = textureHeight / charsPerCol;

        // Create font atlas
        auto atlas = std::make_shared<FontAtlas>();
        atlas->initGrid(textureWidth, textureHeight, charsPerRow, charsPerCol, firstChar);

        // Calculate variable widths for each character (Minecraft algorithm)
        for (uint32_t row = 0; row < charsPerCol; ++row) {
            for (uint32_t col = 0; col < charsPerRow; ++col) {
                uint32_t gridPos = row * charsPerRow + col;
                uint32_t charCode = firstChar + gridPos;

                // Find the actual character width by scanning from right to left
                int actualWidth = findCharacterWidth(textureData, charWidth, charHeight, col, row);

                // Update the character info with the calculated width
                // Width is actualWidth + 1 pixel spacing (Minecraft formula)
                float advance;
                if (charCode == 32) { // Space character
                    advance = 4.0f;
                } else {
                    advance = static_cast<float>(actualWidth + 1);
                }

                auto* charInfo = const_cast<CharInfo*>(atlas->getCharacter(charCode));
                if (charInfo) {
                    charInfo->advance = advance;
                }
            }
        }

        m_fonts[fontName] = atlas;
        m_fontTextures[fontName] = textureIndex;

        spdlog::info("Loaded font '{}' from {} (texture index: {}) with variable widths",
                     fontName, texturePath, textureIndex);

        return textureIndex;
    }

    /**
     * Get a font atlas by name.
     */
    FontAtlas* getFont(const std::string& fontName) {
        auto it = m_fonts.find(fontName);
        if (it != m_fonts.end()) {
            return it->second.get();
        }

        // Try to return default font
        auto defaultIt = m_fonts.find("default");
        if (defaultIt != m_fonts.end()) {
            return defaultIt->second.get();
        }

        return nullptr;
    }

    /**
     * Get the texture index for a font.
     */
    uint32_t getFontTexture(const std::string& fontName) const {
        auto it = m_fontTextures.find(fontName);
        if (it != m_fontTextures.end()) {
            return it->second;
        }

        // Try to return default font texture
        auto defaultIt = m_fontTextures.find("default");
        if (defaultIt != m_fontTextures.end()) {
            return defaultIt->second;
        }

        return 0;
    }

    /**
     * Check if a font is loaded.
     */
    bool hasFont(const std::string& fontName) const {
        return m_fonts.find(fontName) != m_fonts.end();
    }

private:
    /**
     * Find the actual width of a character by scanning pixels from right to left.
     * This implements Minecraft's character width calculation algorithm.
     * @param textureData The loaded texture pixel data
     * @param charWidth Width of each character cell in the grid
     * @param charHeight Height of each character cell in the grid
     * @param charPosX Character's grid X position (column)
     * @param charPosY Character's grid Y position (row)
     * @return The actual width of the character in pixels
     */
    int findCharacterWidth(const TextureData& textureData,
                          uint32_t charWidth, uint32_t charHeight,
                          uint32_t charPosX, uint32_t charPosY) {
        // Scan from right to left (Minecraft algorithm)
        for (int x = static_cast<int>(charWidth) - 1; x >= 0; --x) {
            int pixelX = charPosX * charWidth + x;

            // Check all rows in this column
            for (uint32_t y = 0; y < charHeight; ++y) {
                int pixelY = charPosY * charHeight + y;

                // Get pixel index (RGBA format, 4 bytes per pixel)
                size_t pixelIndex = (pixelY * textureData.width + pixelX) * 4;

                if (pixelIndex + 3 < textureData.pixels.size()) {
                    uint8_t alpha = textureData.pixels[pixelIndex + 3];

                    // Found a non-transparent pixel
                    if (alpha != 0) {
                        return x + 1;
                    }
                }
            }
        }

        // No visible pixels found, return 0
        return 0;
    }

    BindlessTextureManager* m_textureManager = nullptr;
    std::unordered_map<std::string, std::shared_ptr<FontAtlas>> m_fonts;
    std::unordered_map<std::string, uint32_t> m_fontTextures;
};

} // namespace FarHorizon