#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace FarHorizon {

class DepthBuffer {
public:
    void init(VmaAllocator allocator, VkDevice device, uint32_t width, uint32_t height, VkFormat format = VK_FORMAT_D32_SFLOAT);
    void cleanup(VkDevice device, VmaAllocator allocator);
    void resize(VmaAllocator allocator, VkDevice device, uint32_t width, uint32_t height);

    VkImage getImage() const { return m_image; }
    VkImageView getImageView() const { return m_imageView; }
    VkFormat getFormat() const { return m_format; }

private:
    VkImage m_image = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkFormat m_format = VK_FORMAT_D32_SFLOAT;
};

} // namespace FarHorizon