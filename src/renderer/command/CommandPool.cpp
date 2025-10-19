#include "CommandPool.hpp"
#include "../core/VulkanDebug.hpp"

namespace VoxelEngine {

CommandPool::CommandPool(CommandPool&& other) noexcept
    : m_device(other.m_device)
    , m_pool(other.m_pool)
{
    other.m_device = VK_NULL_HANDLE;
    other.m_pool = VK_NULL_HANDLE;
}

CommandPool& CommandPool::operator=(CommandPool&& other) noexcept {
    if (this != &other) {
        cleanup();
        m_device = other.m_device;
        m_pool = other.m_pool;
        other.m_device = VK_NULL_HANDLE;
        other.m_pool = VK_NULL_HANDLE;
    }
    return *this;
}

void CommandPool::init(VkDevice device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags) {
    m_device = device;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = flags;

    VK_CHECK(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_pool));
}

void CommandPool::cleanup() {
    if (m_device != VK_NULL_HANDLE && m_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;
    }
}

void CommandPool::reset() const {
    VK_CHECK(vkResetCommandPool(m_device, m_pool, 0));
}

} // namespace VoxelEngine