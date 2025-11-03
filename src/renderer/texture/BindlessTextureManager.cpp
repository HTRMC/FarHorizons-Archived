#include "BindlessTextureManager.hpp"
#include <stdexcept>
#include <spdlog/spdlog.h>

namespace VoxelEngine {

void BindlessTextureManager::init(VkDevice device, VmaAllocator allocator, uint32_t maxTextures) {
    m_device = device;
    m_allocator = allocator;
    m_maxTextures = maxTextures;

    createDescriptorSetLayout();
    createDescriptorPool();
    createDescriptorSet();
    createSampler();

    spdlog::info("[BindlessTextureManager] Initialized with max {} textures", maxTextures);
}

void BindlessTextureManager::shutdown() {
    // Cleanup all textures (skip external textures - they don't own image/allocation)
    for (auto& texture : m_textures) {
        if (texture.image != VK_NULL_HANDLE && texture.allocation != VK_NULL_HANDLE) {
            texture.cleanup(m_device, m_allocator);
        }
    }
    m_textures.clear();
    m_textureIndices.clear();

    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }

    m_device = VK_NULL_HANDLE;
    m_allocator = VK_NULL_HANDLE;
}

void BindlessTextureManager::createDescriptorSetLayout() {
    // Create a descriptor set layout with a single binding for an array of sampled images
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = m_maxTextures;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = nullptr;

    // Enable descriptor indexing with update after bind and partially bound
    VkDescriptorBindingFlags bindingFlags =
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
    bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.bindingCount = 1;
    bindingFlagsInfo.pBindingFlags = &bindingFlags;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = &bindingFlagsInfo;
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create bindless descriptor set layout");
    }
}

void BindlessTextureManager::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = m_maxTextures;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create bindless descriptor pool");
    }
}

void BindlessTextureManager::createDescriptorSet() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;

    if (vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate bindless descriptor set");
    }
}

void BindlessTextureManager::createSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    // Minecraft-style filtering:
    // - NEAREST magnification = sharp pixels when close (keeps pixel art look)
    // - LINEAR minification = smooth at distance (prevents mosaic patterns)
    // - LINEAR mipmap mode = trilinear filtering (smooth transitions between mip levels)
    samplerInfo.magFilter = VK_FILTER_NEAREST;  // GL_NEAREST - sharp up close
    samplerInfo.minFilter = VK_FILTER_LINEAR;   // Part of GL_LINEAR_MIPMAP_LINEAR
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;  // Trilinear filtering

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;  // Access all mip levels

    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler");
    }
}

void BindlessTextureManager::updateDescriptor(uint32_t index, VkImageView imageView) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = m_sampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = index;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
}

uint32_t BindlessTextureManager::loadTexture(const std::string& filepath, VkCommandBuffer uploadCmd, bool generateMipmaps, uint32_t maxMipLevels) {
    // Check if texture is already loaded
    auto it = m_textureIndices.find(filepath);
    if (it != m_textureIndices.end()) {
        spdlog::info("[BindlessTextureManager] Texture already loaded: {} (index {})",
                     filepath, it->second);
        return it->second;
    }

    // Check if we have space
    if (m_textures.size() >= m_maxTextures) {
        throw std::runtime_error("Bindless texture array is full");
    }

    // Load texture data
    TextureData data = TextureLoader::loadPNG(filepath);

    // Create Vulkan texture with Minecraft-style mipmap levels
    Texture texture = TextureLoader::createTexture(m_device, m_allocator, uploadCmd, data, generateMipmaps, maxMipLevels);

    // Add to array
    uint32_t index = static_cast<uint32_t>(m_textures.size());
    m_textures.push_back(texture);
    m_textureIndices[filepath] = index;

    // Update descriptor
    updateDescriptor(index, texture.imageView);

    spdlog::info("[BindlessTextureManager] Loaded texture: {} (index {})", filepath, index);

    return index;
}

void BindlessTextureManager::reloadTexture(const std::string& filepath, VkCommandBuffer uploadCmd, bool generateMipmaps, uint32_t maxMipLevels) {
    // Find the texture index
    auto it = m_textureIndices.find(filepath);
    if (it == m_textureIndices.end()) {
        spdlog::warn("[BindlessTextureManager] Cannot reload texture - not found: {}", filepath);
        return;
    }

    uint32_t index = it->second;
    spdlog::info("[BindlessTextureManager] Reloading texture: {} (index {})", filepath, index);

    // Get old texture (to clean up after GPU is done)
    Texture oldTexture = m_textures[index];

    // Load texture data
    TextureData data = TextureLoader::loadPNG(filepath);

    // Create new Vulkan texture with new mipmap settings
    Texture newTexture = TextureLoader::createTexture(m_device, m_allocator, uploadCmd, data, generateMipmaps, maxMipLevels);

    // Replace the texture in the array
    m_textures[index] = newTexture;

    // Update descriptor with new image view
    updateDescriptor(index, newTexture.imageView);

    // Clean up old texture (safe because GPU idle was called before this function)
    if (oldTexture.image != VK_NULL_HANDLE && oldTexture.allocation != VK_NULL_HANDLE) {
        oldTexture.cleanup(m_device, m_allocator);
    }

    spdlog::info("[BindlessTextureManager] Reloaded texture: {} (index {}) with {} mip levels",
                 filepath, index, newTexture.mipLevels);
}

uint32_t BindlessTextureManager::registerExternalTexture(VkImageView imageView) {
    // Check if we have space
    if (m_textures.size() >= m_maxTextures) {
        throw std::runtime_error("Bindless texture array is full");
    }

    // Create a placeholder texture entry (we don't own the image/memory, just the view reference)
    Texture texture{};
    texture.imageView = imageView;
    texture.image = VK_NULL_HANDLE;  // External - we don't own it
    texture.allocation = VK_NULL_HANDLE;  // External - we don't own it

    // Add to array
    uint32_t index = static_cast<uint32_t>(m_textures.size());
    m_textures.push_back(texture);

    // Update descriptor
    updateDescriptor(index, imageView);

    spdlog::info("[BindlessTextureManager] Registered external texture (index {})", index);

    return index;
}

void BindlessTextureManager::updateExternalTexture(uint32_t index, VkImageView imageView) {
    // Validate index
    if (index >= m_textures.size()) {
        throw std::runtime_error("Invalid texture index for update");
    }

    // Update the texture entry (note: we don't own the image, just update the view reference)
    m_textures[index].imageView = imageView;

    // Update descriptor
    updateDescriptor(index, imageView);

    spdlog::info("[BindlessTextureManager] Updated external texture (index {})", index);
}

} // namespace VoxelEngine