#pragma once

#include "VulkanDebug.hpp"
#include "VulkanDevice.hpp"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <GLFW/glfw3.h>
#include <string>

namespace FarHorizon {

// Main Vulkan context - owns instance, surface, device, and VMA allocator
class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext() = default;

    // No copy
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    // Move semantics
    VulkanContext(VulkanContext&& other) noexcept;
    VulkanContext& operator=(VulkanContext&& other) noexcept;

    void init(GLFWwindow* window, const std::string& appName);
    void shutdown();

    // Getters
    VkInstance getInstance() const { return instance_; }
    VkSurfaceKHR getSurface() const { return surface_; }
    VulkanDevice& getDevice() { return device_; }
    const VulkanDevice& getDevice() const { return device_; }
    VmaAllocator getAllocator() const { return allocator_; }

    void waitIdle() const;

private:
    void createInstance(const std::string& appName);
    void createSurface(GLFWwindow* window);
    void createAllocator();

    std::vector<const char*> getRequiredExtensions();

private:
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VulkanDebugMessenger debugMessenger_;
    VulkanDevice device_;
    VmaAllocator allocator_ = VK_NULL_HANDLE;
};

} // namespace FarHorizon