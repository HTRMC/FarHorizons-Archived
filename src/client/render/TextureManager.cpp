#include "TextureManager.hpp"
#include "world/ChunkManager.hpp"
#include "core/Settings.hpp"
#include <spdlog/spdlog.h>

namespace FarHorizon {

TextureManager::TextureManager()
    : bindlessTextureManager(std::make_unique<BindlessTextureManager>())
    , fontManager(std::make_unique<FontManager>()) {
}

TextureManager::~TextureManager() = default;

void TextureManager::init(VkDevice device, VmaAllocator allocator, VkCommandBuffer uploadCmd) {
    bindlessTextureManager->init(device, allocator, 1024);
    fontManager->init(bindlessTextureManager.get());
}

void TextureManager::loadBlockTextures(ChunkManager& chunkManager, Settings& settings,
                                      VkCommandBuffer uploadCmd) {
    // Get all textures required by the models
    auto requiredTextures = chunkManager.getRequiredTextures();
    spdlog::info("Found {} unique textures required by block models", requiredTextures.size());

    // Load all required textures
    for (const auto& textureName : requiredTextures) {
        std::string texturePath = "assets/minecraft/textures/block/" + textureName + ".png";
        spdlog::info("Loading texture: {} -> {}", textureName, texturePath);

        // Enable mipmaps for block textures with user's quality setting
        bool enableMipmaps = (settings.mipmapLevels > 0);
        uint32_t textureIndex = bindlessTextureManager->loadTexture(texturePath, uploadCmd,
                                                                     enableMipmaps, settings.mipmapLevels);
        chunkManager.registerTexture(textureName, textureIndex);
    }

    // Cache the loaded textures for potential reloading
    loadedTextures = requiredTextures;

    // Cache texture indices in block models for fast lookup during meshing
    chunkManager.cacheTextureIndices();
}

void TextureManager::reloadTextures(const std::set<std::string>& textureNames,
                                   Settings& settings, VkCommandBuffer uploadCmd) {
    bool enableMipmaps = (settings.mipmapLevels > 0);

    for (const auto& textureName : textureNames) {
        std::string texturePath = "assets/minecraft/textures/block/" + textureName + ".png";
        bindlessTextureManager->reloadTexture(texturePath, uploadCmd, enableMipmaps,
                                             settings.mipmapLevels);
    }

    spdlog::info("Hot reload complete - {} textures reloaded with mipmap level {}",
                 textureNames.size(), settings.mipmapLevels);
}

void TextureManager::loadFonts(VkCommandBuffer uploadCmd) {
    // Try to load the font (won't crash if file doesn't exist, just won't render text)
    fontManager->loadGridFont("default", "assets/minecraft/textures/font/ascii.png",
                             uploadCmd, 128, 128, 16, 16, 0);
}

uint32_t TextureManager::registerExternalTexture(VkImageView imageView) {
    return bindlessTextureManager->registerExternalTexture(imageView);
}

void TextureManager::updateExternalTexture(uint32_t index, VkImageView imageView) {
    bindlessTextureManager->updateExternalTexture(index, imageView);
}

void TextureManager::shutdown() {
    bindlessTextureManager->shutdown();
    loadedTextures.clear();
}

} // namespace FarHorizon