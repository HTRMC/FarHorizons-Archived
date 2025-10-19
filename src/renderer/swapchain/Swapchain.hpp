#pragma once

#include "../core/VulkanContext.hpp"
#include <vulkan/vulkan.h>
#include <vector>

namespace VoxelEngine {

// Swapchain support details
struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

// Swapchain wrapper with automatic recreation on resize
class Swapchain {
public:
    Swapchain() = default;
    ~Swapchain() = default;

    // No copy
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    // Move semantics
    Swapchain(Swapchain&& other) noexcept;
    Swapchain& operator=(Swapchain&& other) noexcept;

    void init(VulkanContext& context, uint32_t width, uint32_t height);
    void shutdown();
    void recreate(uint32_t width, uint32_t height);

    // Acquire next image for rendering
    VkResult acquireNextImage(VkSemaphore signalSemaphore, uint32_t& imageIndex);

    // Present rendered image
    VkResult present(VkQueue presentQueue, VkSemaphore waitSemaphore, uint32_t imageIndex);

    // Getters
    VkSwapchainKHR getSwapchain() const { return m_swapchain; }
    VkFormat getImageFormat() const { return m_imageFormat; }
    VkExtent2D getExtent() const { return m_extent; }
    uint32_t getImageCount() const { return static_cast<uint32_t>(m_images.size()); }
    const std::vector<VkImage>& getImages() const { return m_images; }
    const std::vector<VkImageView>& getImageViews() const { return m_imageViews; }

    // Query swapchain support
    static SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

private:
    void createSwapchain();
    void createImageViews();
    void cleanup();

    // Choose best settings
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

private:
    VulkanContext* m_context = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    VkFormat m_imageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent = {};
};

} // namespace VoxelEngine