#include "VulkanDevice.hpp"
#include "VulkanDebug.hpp"
#include <set>
#include <spdlog/spdlog.h>
#include <algorithm>

namespace VoxelEngine {

VulkanDevice::VulkanDevice(VulkanDevice&& other) noexcept
    : m_physicalDevice(other.m_physicalDevice)
    , m_device(other.m_device)
    , m_surface(other.m_surface)
    , m_queueFamilyIndices(other.m_queueFamilyIndices)
    , m_graphicsQueue(other.m_graphicsQueue)
    , m_presentQueue(other.m_presentQueue)
    , m_computeQueue(other.m_computeQueue)
    , m_transferQueue(other.m_transferQueue)
{
    other.m_physicalDevice = VK_NULL_HANDLE;
    other.m_device = VK_NULL_HANDLE;
    other.m_surface = VK_NULL_HANDLE;
    other.m_graphicsQueue = VK_NULL_HANDLE;
    other.m_presentQueue = VK_NULL_HANDLE;
    other.m_computeQueue = VK_NULL_HANDLE;
    other.m_transferQueue = VK_NULL_HANDLE;
}

VulkanDevice& VulkanDevice::operator=(VulkanDevice&& other) noexcept {
    if (this != &other) {
        m_physicalDevice = other.m_physicalDevice;
        m_device = other.m_device;
        m_surface = other.m_surface;
        m_queueFamilyIndices = other.m_queueFamilyIndices;
        m_graphicsQueue = other.m_graphicsQueue;
        m_presentQueue = other.m_presentQueue;
        m_computeQueue = other.m_computeQueue;
        m_transferQueue = other.m_transferQueue;

        other.m_physicalDevice = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
        other.m_surface = VK_NULL_HANDLE;
        other.m_graphicsQueue = VK_NULL_HANDLE;
        other.m_presentQueue = VK_NULL_HANDLE;
        other.m_computeQueue = VK_NULL_HANDLE;
        other.m_transferQueue = VK_NULL_HANDLE;
    }
    return *this;
}

void VulkanDevice::pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) {
    m_surface = surface;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        spdlog::error("[VulkanDevice] Failed to find GPUs with Vulkan support!");
        assert(false);
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    spdlog::info("[VulkanDevice] Found {} GPU(s)", deviceCount);

    // Score all devices and pick the best one
    int bestScore = -1;
    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;

    for (const auto& device : devices) {
        if (isDeviceSuitable(device, surface)) {
            int score = rateDeviceSuitability(device, surface);

            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(device, &props);
            spdlog::info("[VulkanDevice] {} - Score: {}", props.deviceName, score);

            if (score > bestScore) {
                bestScore = score;
                bestDevice = device;
            }
        }
    }

    if (bestDevice == VK_NULL_HANDLE) {
        spdlog::error("[VulkanDevice] Failed to find a suitable GPU!");
        assert(false);
    }

    m_physicalDevice = bestDevice;
    m_queueFamilyIndices = findQueueFamilies(m_physicalDevice, surface);

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
    spdlog::info("[VulkanDevice] Selected GPU: {}", props.deviceName);
    spdlog::info("[VulkanDevice] API Version: {}.{}.{}",
                 VK_API_VERSION_MAJOR(props.apiVersion),
                 VK_API_VERSION_MINOR(props.apiVersion),
                 VK_API_VERSION_PATCH(props.apiVersion));
}

void VulkanDevice::createLogicalDevice() {
    // Create queue create infos for unique queue families
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        m_queueFamilyIndices.graphicsFamily.value(),
        m_queueFamilyIndices.presentFamily.value()
    };

    if (m_queueFamilyIndices.computeFamily.has_value()) {
        uniqueQueueFamilies.insert(m_queueFamilyIndices.computeFamily.value());
    }
    if (m_queueFamilyIndices.transferFamily.has_value()) {
        uniqueQueueFamilies.insert(m_queueFamilyIndices.transferFamily.value());
    }

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Vulkan 1.1 features (includes shaderDrawParameters for gl_BaseInstance)
    VkPhysicalDeviceVulkan11Features vulkan11Features{};
    vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vulkan11Features.shaderDrawParameters = VK_TRUE;

    // Vulkan 1.2 features (includes descriptor indexing)
    VkPhysicalDeviceVulkan12Features vulkan12Features{};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.descriptorIndexing = VK_TRUE; // Master enable flag for descriptor indexing
    vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;
    vulkan12Features.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    vulkan12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    vulkan12Features.runtimeDescriptorArray = VK_TRUE;
    vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    vulkan12Features.pNext = &vulkan11Features;

    // Vulkan 1.3 features struct (includes dynamicRendering and synchronization2)
    VkPhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.dynamicRendering = VK_TRUE;
    vulkan13Features.synchronization2 = VK_TRUE;
    vulkan13Features.pNext = &vulkan12Features;

    VkPhysicalDeviceFeatures2 deviceFeatures{};
    deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures.features.samplerAnisotropy = VK_TRUE;
    deviceFeatures.features.fillModeNonSolid = VK_TRUE; // Wireframe rendering
    deviceFeatures.features.wideLines = VK_TRUE;
    deviceFeatures.features.multiDrawIndirect = VK_TRUE; // Multi-draw indirect for voxel chunks
    deviceFeatures.pNext = &vulkan13Features;

    // Create logical device
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();
    createInfo.pNext = &deviceFeatures;

    // Enable validation layers on device (for older implementations)
    auto validationLayers = getRequiredValidationLayers();
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();

    VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));

    // Get queue handles
    vkGetDeviceQueue(m_device, m_queueFamilyIndices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_queueFamilyIndices.presentFamily.value(), 0, &m_presentQueue);

    if (m_queueFamilyIndices.computeFamily.has_value()) {
        vkGetDeviceQueue(m_device, m_queueFamilyIndices.computeFamily.value(), 0, &m_computeQueue);
    }
    if (m_queueFamilyIndices.transferFamily.has_value()) {
        vkGetDeviceQueue(m_device, m_queueFamilyIndices.transferFamily.value(), 0, &m_transferQueue);
    }

    spdlog::info("[VulkanDevice] Logical device created");
    spdlog::info("[VulkanDevice] Graphics queue family: {}", m_queueFamilyIndices.graphicsFamily.value());
    spdlog::info("[VulkanDevice] Present queue family: {}", m_queueFamilyIndices.presentFamily.value());
}

void VulkanDevice::shutdown() {
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        spdlog::info("[VulkanDevice] Logical device destroyed");
        m_device = VK_NULL_HANDLE;
    }
    m_physicalDevice = VK_NULL_HANDLE;
}

VkPhysicalDeviceProperties VulkanDevice::getProperties() const {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
    return props;
}

VkPhysicalDeviceFeatures VulkanDevice::getFeatures() const {
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(m_physicalDevice, &features);
    return features;
}

VkPhysicalDeviceMemoryProperties VulkanDevice::getMemoryProperties() const {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
    return memProps;
}

bool VulkanDevice::supportsExtension(const char* extensionName) const {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extensionCount, availableExtensions.data());

    for (const auto& extension : availableExtensions) {
        if (strcmp(extension.extensionName, extensionName) == 0) {
            return true;
        }
    }

    return false;
}

VkFormat VulkanDevice::findSupportedFormat(
    const std::vector<VkFormat>& candidates,
    VkImageTiling tiling,
    VkFormatFeatureFlags features
) const {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    spdlog::error("[VulkanDevice] Failed to find supported format!");
    assert(false);
    return VK_FORMAT_UNDEFINED;
}

VkFormat VulkanDevice::findDepthFormat() const {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

bool VulkanDevice::isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices = findQueueFamilies(device, surface);
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    // Check swapchain support
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        swapChainAdequate = formatCount > 0 && presentModeCount > 0;
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

int VulkanDevice::rateDeviceSuitability(VkPhysicalDevice device, VkSurfaceKHR surface) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);

    int score = 0;

    // Discrete GPUs have a significant performance advantage
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    // Maximum possible size of textures affects graphics quality
    score += props.limits.maxImageDimension2D;

    // Check for required features
    if (!features.samplerAnisotropy) {
        return 0;
    }

    return score;
}

QueueFamilyIndices VulkanDevice::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        const auto& queueFamily = queueFamilies[i];

        // Graphics queue
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        // Present queue
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        // Compute queue (prefer dedicated)
        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            if (!indices.computeFamily.has_value() ||
                !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                indices.computeFamily = i;
            }
        }

        // Transfer queue (prefer dedicated)
        if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
            if (!indices.transferFamily.has_value() ||
                !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                indices.transferFamily = i;
            }
        }

        if (indices.isComplete()) {
            break;
        }
    }

    return indices;
}

bool VulkanDevice::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(m_deviceExtensions.begin(), m_deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

} // namespace VoxelEngine