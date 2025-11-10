#pragma once

#include <vulkan/vulkan.h>

namespace FarHorizon {

// RAII wrapper for VkSemaphore
class Semaphore {
public:
    Semaphore() = default;
    ~Semaphore() { cleanup(); }

    // No copy
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

    // Move semantics
    Semaphore(Semaphore&& other) noexcept;
    Semaphore& operator=(Semaphore&& other) noexcept;

    void init(VkDevice device);
    void cleanup();

    VkSemaphore getSemaphore() const { return m_semaphore; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkSemaphore m_semaphore = VK_NULL_HANDLE;
};

} // namespace FarHorizon