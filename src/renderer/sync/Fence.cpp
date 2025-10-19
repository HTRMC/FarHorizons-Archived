#include "Fence.hpp"
#include "../core/VulkanDebug.hpp"

namespace VoxelEngine {

Fence::Fence(Fence&& other) noexcept
    : m_device(other.m_device)
    , m_fence(other.m_fence)
{
    other.m_device = VK_NULL_HANDLE;
    other.m_fence = VK_NULL_HANDLE;
}

Fence& Fence::operator=(Fence&& other) noexcept {
    if (this != &other) {
        cleanup();
        m_device = other.m_device;
        m_fence = other.m_fence;
        other.m_device = VK_NULL_HANDLE;
        other.m_fence = VK_NULL_HANDLE;
    }
    return *this;
}

void Fence::init(VkDevice device, bool signaled) {
    m_device = device;

    VkFenceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    createInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

    VK_CHECK(vkCreateFence(m_device, &createInfo, nullptr, &m_fence));
}

void Fence::cleanup() {
    if (m_device != VK_NULL_HANDLE && m_fence != VK_NULL_HANDLE) {
        vkDestroyFence(m_device, m_fence, nullptr);
        m_fence = VK_NULL_HANDLE;
    }
}

void Fence::wait(uint64_t timeout) const {
    VK_CHECK(vkWaitForFences(m_device, 1, &m_fence, VK_TRUE, timeout));
}

void Fence::reset() const {
    VK_CHECK(vkResetFences(m_device, 1, &m_fence));
}

bool Fence::isSignaled() const {
    return vkGetFenceStatus(m_device, m_fence) == VK_SUCCESS;
}

} // namespace VoxelEngine