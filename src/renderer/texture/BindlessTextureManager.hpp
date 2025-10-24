#pragma once

#include "TextureLoader.hpp"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace VoxelEngine {

// Manages a bindless texture array using descriptor indexing
class BindlessTextureManager {
public:
    BindlessTextureManager() = default;
    ~BindlessTextureManager() = default;

    // No copy
    BindlessTextureManager(const BindlessTextureManager&) = delete;
    BindlessTextureManager& operator=(const BindlessTextureManager&) = delete;

    // Initialize with max texture count
    void init(VkDevice device, VmaAllocator allocator, uint32_t maxTextures = 1024);
    void shutdown();

    // Load texture from file and return its index in the bindless array
    uint32_t loadTexture(const std::string& filepath, VkCommandBuffer uploadCmd, bool generateMipmaps = true);

    // Get descriptor set for binding
    VkDescriptorSet getDescriptorSet() const { return m_descriptorSet; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }
    VkSampler getSampler() const { return m_sampler; }

    // Get texture count
    uint32_t getTextureCount() const { return static_cast<uint32_t>(m_textures.size()); }

private:
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSet();
    void createSampler();
    void updateDescriptor(uint32_t index, VkImageView imageView);

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VmaAllocator m_allocator = VK_NULL_HANDLE;

    uint32_t m_maxTextures = 0;
    std::vector<Texture> m_textures;
    std::unordered_map<std::string, uint32_t> m_textureIndices; // filepath -> index

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
};

} // namespace VoxelEngine