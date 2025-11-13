#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace FarHorizon {

class DepthBuffer {
public:
    void init(VmaAllocator allocator, VkDevice device, uint32_t width, uint32_t height, VkFormat format = VK_FORMAT_D32_SFLOAT);
    void cleanup(VkDevice device, VmaAllocator allocator);
    void resize(VmaAllocator allocator, VkDevice device, uint32_t width, uint32_t height);

    VkImage getImage() const { return image_; }
    VkImageView getImageView() const { return imageView_; }
    VkFormat getFormat() const { return format_; }

private:
    VkImage image_ = VK_NULL_HANDLE;
    VkImageView imageView_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;
    VkFormat format_ = VK_FORMAT_D32_SFLOAT;
};

} // namespace FarHorizon