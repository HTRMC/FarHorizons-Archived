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
    VkInstance getInstance() const { return m_instance; }
    VkSurfaceKHR getSurface() const { return m_surface; }
    VulkanDevice& getDevice() { return m_device; }
    const VulkanDevice& getDevice() const { return m_device; }
    VmaAllocator getAllocator() const { return m_allocator; }

    void waitIdle() const;

private:
    void createInstance(const std::string& appName);
    void createSurface(GLFWwindow* window);
    void createAllocator();

    std::vector<const char*> getRequiredExtensions();

private:
    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VulkanDebugMessenger m_debugMessenger;
    VulkanDevice m_device;
    VmaAllocator m_allocator = VK_NULL_HANDLE;
};

} // namespace FarHorizon