#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

namespace VoxelEngine {

// Queue family indices
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;
    std::optional<uint32_t> transferFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

// Physical device wrapper with scoring system
class VulkanDevice {
public:
    VulkanDevice() = default;
    ~VulkanDevice() = default;

    // No copy
    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;

    // Move semantics
    VulkanDevice(VulkanDevice&& other) noexcept;
    VulkanDevice& operator=(VulkanDevice&& other) noexcept;

    void pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
    void createLogicalDevice();
    void shutdown();

    // Getters
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    VkDevice getLogicalDevice() const { return m_device; }
    const QueueFamilyIndices& getQueueFamilyIndices() const { return m_queueFamilyIndices; }

    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue getPresentQueue() const { return m_presentQueue; }
    VkQueue getComputeQueue() const { return m_computeQueue; }
    VkQueue getTransferQueue() const { return m_transferQueue; }

    VkPhysicalDeviceProperties getProperties() const;
    VkPhysicalDeviceFeatures getFeatures() const;
    VkPhysicalDeviceMemoryProperties getMemoryProperties() const;

    // Query support
    bool supportsExtension(const char* extensionName) const;
    VkFormat findSupportedFormat(
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    ) const;

    VkFormat findDepthFormat() const;

private:
    // Physical device selection
    bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
    int rateDeviceSuitability(VkPhysicalDevice device, VkSurfaceKHR surface);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

private:
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    QueueFamilyIndices m_queueFamilyIndices;

    // Queues
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    VkQueue m_computeQueue = VK_NULL_HANDLE;
    VkQueue m_transferQueue = VK_NULL_HANDLE;

    // Required device extensions
    const std::vector<const char*> m_deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    };
};

} // namespace VoxelEngine