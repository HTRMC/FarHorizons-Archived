#include "VulkanDebug.hpp"
#include <vector>
#include <spdlog/spdlog.h>

namespace VoxelEngine {

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
) {
    // Color codes for different severity levels
    const char* color = "";
    const char* reset = "\033[0m";
    const char* severityStr = "";

    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            color = "\033[37m"; // White
            severityStr = "VERBOSE";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            color = "\033[36m"; // Cyan
            severityStr = "INFO";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            color = "\033[33m"; // Yellow
            severityStr = "WARNING";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            color = "\033[31m"; // Red
            severityStr = "ERROR";
            break;
        default:
            severityStr = "UNKNOWN";
            break;
    }

    // Map Vulkan severity to spdlog levels
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            spdlog::debug("[VULKAN] {}", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            spdlog::info("[VULKAN] {}", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            spdlog::warn("[VULKAN] {}", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            spdlog::error("[VULKAN] {}", pCallbackData->pMessage);
            break;
        default:
            spdlog::info("[VULKAN] {}", pCallbackData->pMessage);
            break;
    }

    // Return VK_TRUE to abort the call that triggered the validation layer message
    // Only abort on errors in debug builds
    return VK_FALSE;
}

void VulkanDebugMessenger::init(VkInstance instance) {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;

    // Load the extension function
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT"
    );

    if (func != nullptr) {
        VK_CHECK(func(instance, &createInfo, nullptr, &m_debugMessenger));
        spdlog::info("[VulkanDebug] Debug messenger created");
    } else {
        spdlog::error("[VulkanDebug] Failed to load vkCreateDebugUtilsMessengerEXT");
    }
}

void VulkanDebugMessenger::shutdown(VkInstance instance) {
    if (m_debugMessenger != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkDestroyDebugUtilsMessengerEXT"
        );

        if (func != nullptr) {
            func(instance, m_debugMessenger, nullptr);
            spdlog::info("[VulkanDebug] Debug messenger destroyed");
        }

        m_debugMessenger = VK_NULL_HANDLE;
    }
}

bool checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    auto requiredLayers = getRequiredValidationLayers();

    for (const char* layerName : requiredLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            spdlog::error("[VulkanDebug] Validation layer not found: {}", layerName);
            return false;
        }
    }

    spdlog::info("[VulkanDebug] All validation layers supported");
    return true;
}

} // namespace VoxelEngine