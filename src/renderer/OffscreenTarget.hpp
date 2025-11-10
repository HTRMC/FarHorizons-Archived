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
        m_device = device;
        m_allocator = allocator;
        m_width = width;
        m_height = height;
        m_colorFormat = colorFormat;
        m_depthFormat = depthFormat;

        createColorImage();
        if (m_depthFormat != VK_FORMAT_UNDEFINED) {
            createDepthImage();
        }
    }

    void resize(uint32_t width, uint32_t height) {
        if (width == m_width && height == m_height) return;

        cleanup();
        m_width = width;
        m_height = height;
        createColorImage();
        if (m_depthFormat != VK_FORMAT_UNDEFINED) {
            createDepthImage();
        }
    }

    void cleanup() {
        if (m_colorImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, m_colorImageView, nullptr);
            m_colorImageView = VK_NULL_HANDLE;
        }
        if (m_colorImage != VK_NULL_HANDLE) {
            vmaDestroyImage(m_allocator, m_colorImage, m_colorAllocation);
            m_colorImage = VK_NULL_HANDLE;
            m_colorAllocation = VK_NULL_HANDLE;
        }
        if (m_depthImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, m_depthImageView, nullptr);
            m_depthImageView = VK_NULL_HANDLE;
        }
        if (m_depthImage != VK_NULL_HANDLE) {
            vmaDestroyImage(m_allocator, m_depthImage, m_depthAllocation);
            m_depthImage = VK_NULL_HANDLE;
            m_depthAllocation = VK_NULL_HANDLE;
        }
    }

    VkImageView getColorImageView() const { return m_colorImageView; }
    VkImageView getDepthImageView() const { return m_depthImageView; }
    VkImage getColorImage() const { return m_colorImage; }
    VkImage getDepthImage() const { return m_depthImage; }
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }
    VkFormat getColorFormat() const { return m_colorFormat; }

private:
    void createColorImage() {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_width;
        imageInfo.extent.height = m_height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_colorFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if (vmaCreateImage(m_allocator, &imageInfo, &allocInfo, &m_colorImage, &m_colorAllocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create offscreen color image");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_colorImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_colorFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_colorImageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create offscreen color image view");
        }
    }

    void createDepthImage() {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_width;
        imageInfo.extent.height = m_height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_depthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if (vmaCreateImage(m_allocator, &imageInfo, &allocInfo, &m_depthImage, &m_depthAllocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create offscreen depth image");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_depthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_depthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_depthImageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create offscreen depth image view");
        }
    }

    VkDevice m_device = VK_NULL_HANDLE;
    VmaAllocator m_allocator = VK_NULL_HANDLE;

    VkImage m_colorImage = VK_NULL_HANDLE;
    VmaAllocation m_colorAllocation = VK_NULL_HANDLE;
    VkImageView m_colorImageView = VK_NULL_HANDLE;

    VkImage m_depthImage = VK_NULL_HANDLE;
    VmaAllocation m_depthAllocation = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    VkFormat m_colorFormat = VK_FORMAT_UNDEFINED;
    VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;
};

} // namespace FarHorizon