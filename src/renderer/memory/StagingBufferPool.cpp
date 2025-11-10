#include "StagingBufferPool.hpp"
#include "../core/VulkanDebug.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace FarHorizon {

void StagingBufferPool::init(VmaAllocator allocator, VkDeviceSize defaultBufferSize) {
    m_allocator = allocator;
    m_defaultBufferSize = defaultBufferSize;
    spdlog::info("[StagingBufferPool] Initialized with default buffer size: {} MB",
                 defaultBufferSize / (1024 * 1024));
}

void StagingBufferPool::cleanup() {
    if (m_allocator != VK_NULL_HANDLE) {
        for (auto& entry : m_pool) {
            entry.buffer.cleanup();
        }
        m_pool.clear();
        m_allocator = VK_NULL_HANDLE;
        spdlog::info("[StagingBufferPool] Cleaned up");
    }
}

StagingBuffer* StagingBufferPool::acquire(VkDeviceSize minSize) {
    // First, try to find an available buffer that's large enough
    for (auto& entry : m_pool) {
        if (!entry.inUse && entry.buffer.getSize() >= minSize) {
            entry.inUse = true;
            return &entry.buffer;
        }
    }

    // No suitable buffer found, create a new one
    VkDeviceSize bufferSize = std::max(minSize, m_defaultBufferSize);

    PoolEntry newEntry;
    newEntry.buffer.init(m_allocator, bufferSize);
    newEntry.inUse = true;

    m_pool.push_back(std::move(newEntry));

    spdlog::info("[StagingBufferPool] Created new staging buffer of size: {} MB (pool size: {})",
                 bufferSize / (1024 * 1024), m_pool.size());

    return &m_pool.back().buffer;
}

void StagingBufferPool::release(StagingBuffer* buffer) {
    for (auto& entry : m_pool) {
        if (&entry.buffer == buffer) {
            entry.inUse = false;
            return;
        }
    }

    spdlog::warn("[StagingBufferPool] Warning: Tried to release buffer not in pool!");
}

void StagingBufferPool::reset() {
    // Mark all buffers as available
    for (auto& entry : m_pool) {
        entry.inUse = false;
    }
}

} // namespace FarHorizon