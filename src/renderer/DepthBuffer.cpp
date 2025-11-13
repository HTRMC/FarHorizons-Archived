#include "DepthBuffer.hpp"

namespace FarHorizon {

void DepthBuffer::init(VmaAllocator allocator, VkDevice device, uint32_t width, uint32_t height, VkFormat format) {
    format_ = format;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format_;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(allocator, &imageInfo, &allocInfo, &image_, &allocation_, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format_;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    vkCreateImageView(device, &viewInfo, nullptr, &imageView_);
}

void DepthBuffer::cleanup(VkDevice device, VmaAllocator allocator) {
    if (imageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device, imageView_, nullptr);
        imageView_ = VK_NULL_HANDLE;
    }

    if (image_ != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator, image_, allocation_);
        image_ = VK_NULL_HANDLE;
        allocation_ = VK_NULL_HANDLE;
    }
}

void DepthBuffer::resize(VmaAllocator allocator, VkDevice device, uint32_t width, uint32_t height) {
    cleanup(device, allocator);
    init(allocator, device, width, height, format_);
}

} // namespace FarHorizon