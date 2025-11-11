#pragma once

#include <memory>
#include <string>
#include <vector>
#include <set>
#include "renderer/texture/BindlessTextureManager.hpp"
#include "text/FontManager.hpp"
#include "world/ChunkManager.hpp"
#include "core/Settings.hpp"

namespace FarHorizon {

/**
 * Manages all texture and font resources.
 * Similar to Minecraft's TextureManager + FontManager.
 */
class TextureManager {
public:
    TextureManager();
    ~TextureManager();

    /**
     * Initialize texture manager
     */
    void init(VkDevice device, VmaAllocator allocator, VkCommandBuffer uploadCmd);

    /**
     * Load all block textures required by chunk manager
     */
    void loadBlockTextures(ChunkManager& chunkManager, Settings& settings,
                          VkCommandBuffer uploadCmd);

    /**
     * Reload all textures (used when mipmap settings change)
     */
    void reloadTextures(const std::set<std::string>& textureNames,
                       Settings& settings, VkCommandBuffer uploadCmd);

    /**
     * Load fonts
     */
    void loadFonts(VkCommandBuffer uploadCmd);

    /**
     * Register external texture (for offscreen render targets)
     */
    uint32_t registerExternalTexture(VkImageView imageView);

    /**
     * Update external texture after resize
     */
    void updateExternalTexture(uint32_t index, VkImageView imageView);

    /**
     * Shutdown and cleanup
     */
    void shutdown();

    // Getters
    BindlessTextureManager& getBindlessTextureManager() { return *bindlessTextureManager; }
    FontManager& getFontManager() { return *fontManager; }

    VkDescriptorSetLayout getDescriptorSetLayout() const {
        return bindlessTextureManager->getDescriptorSetLayout();
    }

    VkDescriptorSet getDescriptorSet() const {
        return bindlessTextureManager->getDescriptorSet();
    }

    bool hasFont(const std::string& name) const {
        return fontManager->hasFont(name);
    }

private:
    std::unique_ptr<BindlessTextureManager> bindlessTextureManager;
    std::unique_ptr<FontManager> fontManager;

    std::vector<std::string> loadedTextures;
};

} // namespace FarHorizon
