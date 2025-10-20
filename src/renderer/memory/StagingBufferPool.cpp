#include "StagingBufferPool.hpp"
#include "../core/VulkanDebug.hpp"
#include <iostream>
#include <algorithm>

namespace VoxelEngine {

void StagingBufferPool::init(VmaAllocator allocator, VkDeviceSize defaultBufferSize) {
    m_allocator = allocator;
    m_defaultBufferSize = defaultBufferSize;
    std::cout << "[StagingBufferPool] Initialized with default buffer size: "
              << (defaultBufferSize / (1024 * 1024)) << " MB" << std::endl;
}

void StagingBufferPool::cleanup() {
    for (auto& entry : m_pool) {
        entry.buffer.cleanup();
    }
    m_pool.clear();
    std::cout << "[StagingBufferPool] Cleaned up" << std::endl;
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

    std::cout << "[StagingBufferPool] Created new staging buffer of size: "
              << (bufferSize / (1024 * 1024)) << " MB (pool size: " << m_pool.size() << ")" << std::endl;

    return &m_pool.back().buffer;
}

void StagingBufferPool::release(StagingBuffer* buffer) {
    for (auto& entry : m_pool) {
        if (&entry.buffer == buffer) {
            entry.inUse = false;
            return;
        }
    }

    std::cerr << "[StagingBufferPool] Warning: Tried to release buffer not in pool!" << std::endl;
}

void StagingBufferPool::reset() {
    // Mark all buffers as available
    for (auto& entry : m_pool) {
        entry.inUse = false;
    }
}

} // namespace VoxelEngine