#include "Semaphore.hpp"
#include "../core/VulkanDebug.hpp"

namespace VoxelEngine {

Semaphore::Semaphore(Semaphore&& other) noexcept
    : m_device(other.m_device)
    , m_semaphore(other.m_semaphore)
{
    other.m_device = VK_NULL_HANDLE;
    other.m_semaphore = VK_NULL_HANDLE;
}

Semaphore& Semaphore::operator=(Semaphore&& other) noexcept {
    if (this != &other) {
        cleanup();
        m_device = other.m_device;
        m_semaphore = other.m_semaphore;
        other.m_device = VK_NULL_HANDLE;
        other.m_semaphore = VK_NULL_HANDLE;
    }
    return *this;
}

void Semaphore::init(VkDevice device) {
    m_device = device;

    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VK_CHECK(vkCreateSemaphore(m_device, &createInfo, nullptr, &m_semaphore));
}

void Semaphore::cleanup() {
    if (m_device != VK_NULL_HANDLE && m_semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(m_device, m_semaphore, nullptr);
        m_semaphore = VK_NULL_HANDLE;
    }
}

} // namespace VoxelEngine