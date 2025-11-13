#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <stdexcept>

namespace FarHorizon {

/**
 * Offscreen render target for post-processing effects.
 * Contains color and depth attachments for rendering to texture.
 */
class OffscreenTarget {
public:
    OffscreenTarget() = default;

    void init(VmaAllocator allocator, VkDevice device, uint32_t width, uint32_t height, VkFormat colorFormat, VkFormat depthFormat) {
        device_ = device;
        allocator_ = allocator;
        width_ = width;
        height_ = height;
        colorFormat_ = colorFormat;
        depthFormat_ = depthFormat;

        createColorImage();
        if (depthFormat_ != VK_FORMAT_UNDEFINED) {
            createDepthImage();
        }
    }

    void resize(uint32_t width, uint32_t height) {
        if (width == width_ && height == height_) return;

        cleanup();
        width_ = width;
        height_ = height;
        createColorImage();
        if (depthFormat_ != VK_FORMAT_UNDEFINED) {
            createDepthImage();
        }
    }

    void cleanup() {
        if (colorImageView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, colorImageView_, nullptr);
            colorImageView_ = VK_NULL_HANDLE;
        }
        if (colorImage_ != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator_, colorImage_, colorAllocation_);
            colorImage_ = VK_NULL_HANDLE;
            colorAllocation_ = VK_NULL_HANDLE;
        }
        if (depthImageView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, depthImageView_, nullptr);
            depthImageView_ = VK_NULL_HANDLE;
        }
        if (depthImage_ != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator_, depthImage_, depthAllocation_);
            depthImage_ = VK_NULL_HANDLE;
            depthAllocation_ = VK_NULL_HANDLE;
        }
    }

    VkImageView getColorImageView() const { return colorImageView_; }
    VkImageView getDepthImageView() const { return depthImageView_; }
    VkImage getColorImage() const { return colorImage_; }
    VkImage getDepthImage() const { return depthImage_; }
    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }
    VkFormat getColorFormat() const { return colorFormat_; }

private:
    void createColorImage() {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width_;
        imageInfo.extent.height = height_;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = colorFormat_;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if (vmaCreateImage(allocator_, &imageInfo, &allocInfo, &colorImage_, &colorAllocation_, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create offscreen color image");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = colorImage_;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = colorFormat_;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device_, &viewInfo, nullptr, &colorImageView_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create offscreen color image view");
        }
    }

    void createDepthImage() {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width_;
        imageInfo.extent.height = height_;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = depthFormat_;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if (vmaCreateImage(allocator_, &imageInfo, &allocInfo, &depthImage_, &depthAllocation_, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create offscreen depth image");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depthImage_;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = depthFormat_;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device_, &viewInfo, nullptr, &depthImageView_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create offscreen depth image view");
        }
    }

    VkDevice device_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = VK_NULL_HANDLE;

    VkImage colorImage_ = VK_NULL_HANDLE;
    VmaAllocation colorAllocation_ = VK_NULL_HANDLE;
    VkImageView colorImageView_ = VK_NULL_HANDLE;

    VkImage depthImage_ = VK_NULL_HANDLE;
    VmaAllocation depthAllocation_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;

    uint32_t width_ = 0;
    uint32_t height_ = 0;
    VkFormat colorFormat_ = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
};

} // namespace FarHorizon