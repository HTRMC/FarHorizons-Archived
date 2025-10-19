#pragma once

#include <vulkan/vulkan.h>

namespace VoxelEngine {

// RAII wrapper for VkFence
class Fence {
public:
    Fence() = default;
    ~Fence() { cleanup(); }

    // No copy
    Fence(const Fence&) = delete;
    Fence& operator=(const Fence&) = delete;

    // Move semantics
    Fence(Fence&& other) noexcept;
    Fence& operator=(Fence&& other) noexcept;

    void init(VkDevice device, bool signaled = false);
    void cleanup();

    void wait(uint64_t timeout = UINT64_MAX) const;
    void reset() const;
    bool isSignaled() const;

    VkFence getFence() const { return m_fence; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkFence m_fence = VK_NULL_HANDLE;
};

} // namespace VoxelEngine