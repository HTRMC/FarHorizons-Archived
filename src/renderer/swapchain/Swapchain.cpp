#include "Swapchain.hpp"
#include "../core/VulkanDebug.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace FarHorizon {

Swapchain::Swapchain(Swapchain&& other) noexcept
    : m_context(other.m_context)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_swapchain(other.m_swapchain)
    , m_images(std::move(other.m_images))
    , m_imageViews(std::move(other.m_imageViews))
    , m_imageFormat(other.m_imageFormat)
    , m_extent(other.m_extent)
{
    other.m_context = nullptr;
    other.m_swapchain = VK_NULL_HANDLE;
    other.m_imageFormat = VK_FORMAT_UNDEFINED;
}

Swapchain& Swapchain::operator=(Swapchain&& other) noexcept {
    if (this != &other) {
        cleanup();

        m_context = other.m_context;
        m_width = other.m_width;
        m_height = other.m_height;
        m_swapchain = other.m_swapchain;
        m_images = std::move(other.m_images);
        m_imageViews = std::move(other.m_imageViews);
        m_imageFormat = other.m_imageFormat;
        m_extent = other.m_extent;

        other.m_context = nullptr;
        other.m_swapchain = VK_NULL_HANDLE;
        other.m_imageFormat = VK_FORMAT_UNDEFINED;
    }
    return *this;
}

void Swapchain::init(VulkanContext& context, uint32_t width, uint32_t height) {
    m_context = &context;
    m_width = width;
    m_height = height;

    createSwapchain();
    createImageViews();

    spdlog::info("[Swapchain] Created with {} images ({}x{})",
                 m_images.size(), m_extent.width, m_extent.height);
}

void Swapchain::shutdown() {
    cleanup();
    m_context = nullptr;
}

void Swapchain::recreate(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    // Wait for device to finish
    m_context->waitIdle();

    // Clean up old swapchain
    cleanup();

    // Create new swapchain
    createSwapchain();
    createImageViews();

    spdlog::info("[Swapchain] Recreated ({}x{})", m_extent.width, m_extent.height);
}

VkResult Swapchain::acquireNextImage(VkSemaphore signalSemaphore, uint32_t& imageIndex) {
    return vkAcquireNextImageKHR(
        m_context->getDevice().getLogicalDevice(),
        m_swapchain,
        UINT64_MAX, // Timeout (infinite)
        signalSemaphore,
        VK_NULL_HANDLE,
        &imageIndex
    );
}

VkResult Swapchain::present(VkQueue presentQueue, VkSemaphore waitSemaphore, uint32_t imageIndex) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &waitSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    return vkQueuePresentKHR(presentQueue, &presentInfo);
}

SwapchainSupportDetails Swapchain::querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapchainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

void Swapchain::createSwapchain() {
    SwapchainSupportDetails swapchainSupport = querySwapchainSupport(
        m_context->getDevice().getPhysicalDevice(),
        m_context->getSurface()
    );

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities);

    // Request one more image than minimum to implement triple buffering
    uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
    if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount) {
        imageCount = swapchainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_context->getSurface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto& queueFamilyIndices = m_context->getDevice().getQueueFamilyIndices();
    uint32_t queueFamilyIndicesArray[] = {
        queueFamilyIndices.graphicsFamily.value(),
        queueFamilyIndices.presentFamily.value()
    };

    if (queueFamilyIndices.graphicsFamily != queueFamilyIndices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndicesArray;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(m_context->getDevice().getLogicalDevice(), &createInfo, nullptr, &m_swapchain));

    // Retrieve swapchain images
    vkGetSwapchainImagesKHR(m_context->getDevice().getLogicalDevice(), m_swapchain, &imageCount, nullptr);
    m_images.resize(imageCount);
    vkGetSwapchainImagesKHR(m_context->getDevice().getLogicalDevice(), m_swapchain, &imageCount, m_images.data());

    m_imageFormat = surfaceFormat.format;
    m_extent = extent;
}

void Swapchain::createImageViews() {
    m_imageViews.resize(m_images.size());

    for (size_t i = 0; i < m_images.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_imageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(m_context->getDevice().getLogicalDevice(), &createInfo, nullptr, &m_imageViews[i]));
    }
}

void Swapchain::cleanup() {
    if (m_context && m_context->getDevice().getLogicalDevice() != VK_NULL_HANDLE) {
        for (auto imageView : m_imageViews) {
            vkDestroyImageView(m_context->getDevice().getLogicalDevice(), imageView, nullptr);
        }
        m_imageViews.clear();

        if (m_swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(m_context->getDevice().getLogicalDevice(), m_swapchain, nullptr);
            spdlog::info("[Swapchain] Destroyed");
            m_swapchain = VK_NULL_HANDLE;
        }

        m_images.clear();
    }
}

VkSurfaceFormatKHR Swapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    // Prefer SRGB if available
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    // Otherwise just use the first format
    return availableFormats[0];
}

VkPresentModeKHR Swapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    // Prefer mailbox (triple buffering) if available
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    // FIFO is always available (vsync)
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Swapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        VkExtent2D actualExtent = { m_width, m_height };

        actualExtent.width = std::clamp(actualExtent.width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

} // namespace FarHorizon