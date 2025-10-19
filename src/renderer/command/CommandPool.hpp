#pragma once

#include <vulkan/vulkan.h>

namespace VoxelEngine {

// RAII wrapper for VkCommandPool
class CommandPool {
public:
    CommandPool() = default;
    ~CommandPool() { cleanup(); }

    // No copy
    CommandPool(const CommandPool&) = delete;
    CommandPool& operator=(const CommandPool&) = delete;

    // Move semantics
    CommandPool(CommandPool&& other) noexcept;
    CommandPool& operator=(CommandPool&& other) noexcept;

    void init(VkDevice device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);
    void cleanup();
    void reset() const;

    VkCommandPool getPool() const { return m_pool; }
    VkDevice getDevice() const { return m_device; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkCommandPool m_pool = VK_NULL_HANDLE;
};

} // namespace VoxelEngine