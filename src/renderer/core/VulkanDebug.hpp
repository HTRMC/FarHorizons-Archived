#pragma once

#include <vulkan/vulkan.h>
#include <magic_enum/magic_enum.hpp>
#include <string>
#include <cassert>
#include <iostream>

namespace VoxelEngine {

// AAA-standard Vulkan error checking macro
#define VK_CHECK(result) \
    do { \
        VkResult _vk_result = (result); \
        if (_vk_result != VK_SUCCESS) { \
            std::cerr << "[VULKAN ERROR] " << magic_enum::enum_name(_vk_result) \
                      << " in " << __FILE__ << ":" << __LINE__ << std::endl; \
            assert(false && "Vulkan API call failed"); \
        } \
    } while (0)

// Validation layer debug callback
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
);

// Validation layer setup
class VulkanDebugMessenger {
public:
    VulkanDebugMessenger() = default;
    ~VulkanDebugMessenger() = default;

    // No copy
    VulkanDebugMessenger(const VulkanDebugMessenger&) = delete;
    VulkanDebugMessenger& operator=(const VulkanDebugMessenger&) = delete;

    // Move semantics
    VulkanDebugMessenger(VulkanDebugMessenger&& other) noexcept
        : m_debugMessenger(other.m_debugMessenger)
    {
        other.m_debugMessenger = VK_NULL_HANDLE;
    }

    VulkanDebugMessenger& operator=(VulkanDebugMessenger&& other) noexcept {
        if (this != &other) {
            m_debugMessenger = other.m_debugMessenger;
            other.m_debugMessenger = VK_NULL_HANDLE;
        }
        return *this;
    }

    void init(VkInstance instance);
    void shutdown(VkInstance instance);

    VkDebugUtilsMessengerEXT getMessenger() const { return m_debugMessenger; }

private:
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
};

// Required validation layers
inline const std::vector<const char*> getRequiredValidationLayers() {
    return {
        "VK_LAYER_KHRONOS_validation"
    };
}

// Check if validation layers are available
bool checkValidationLayerSupport();

} // namespace VoxelEngine