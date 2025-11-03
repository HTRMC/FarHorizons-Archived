#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <string>
#include <vector>
#include <cstdint>

namespace VoxelEngine {

// Texture data loaded from file
struct TextureData {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    std::vector<uint8_t> pixels;

    bool isValid() const {
        return width > 0 && height > 0 && !pixels.empty();
    }
};

// Vulkan texture resource
struct Texture {
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipLevels = 1;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingAllocation = VK_NULL_HANDLE;

    void cleanup(VkDevice device, VmaAllocator allocator) {
        if (stagingBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
            stagingBuffer = VK_NULL_HANDLE;
            stagingAllocation = VK_NULL_HANDLE;
        }
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, imageView, nullptr);
            imageView = VK_NULL_HANDLE;
        }
        if (image != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator, image, allocation);
            image = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
        }
    }
};

class TextureLoader {
public:
    // Load PNG file using libspng
    static TextureData loadPNG(const std::string& filepath);

    // Create Vulkan texture from texture data
    static Texture createTexture(
        VkDevice device,
        VmaAllocator allocator,
        VkCommandBuffer uploadCmd,
        const TextureData& data,
        bool generateMipmaps = true,
        uint32_t maxMipLevels = 0  // 0 = auto (full chain), 1-4 = limit mip levels
    );

    // Calculate mip levels for a texture
    // maxMipLevels: 0 = unlimited (full chain), 1-4 = Minecraft-style limit
    static uint32_t calculateMipLevels(uint32_t width, uint32_t height, uint32_t maxMipLevels = 0);

private:
    // Generate mipmaps for a texture
    static void generateMipmaps(
        VkCommandBuffer cmd,
        VkImage image,
        VkFormat format,
        uint32_t width,
        uint32_t height,
        uint32_t mipLevels
    );
};

} // namespace VoxelEngine