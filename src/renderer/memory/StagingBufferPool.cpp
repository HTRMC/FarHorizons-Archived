#include "StagingBufferPool.hpp"
#include "../core/VulkanDebug.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace FarHorizon {

void StagingBufferPool::init(VmaAllocator allocator, VkDeviceSize defaultBufferSize) {
    allocator_ = allocator;
    defaultBufferSize_ = defaultBufferSize;
    spdlog::info("[StagingBufferPool] Initialized with default buffer size: {} MB",
                 defaultBufferSize / (1024 * 1024));
}

void StagingBufferPool::cleanup() {
    if (allocator_ != VK_NULL_HANDLE) {
        for (auto& entry : pool_) {
            entry.buffer.cleanup();
        }
        pool_.clear();
        allocator_ = VK_NULL_HANDLE;
        spdlog::info("[StagingBufferPool] Cleaned up");
    }
}

StagingBuffer* StagingBufferPool::acquire(VkDeviceSize minSize) {
    // First, try to find an available buffer that's large enough
    for (auto& entry : pool_) {
        if (!entry.inUse && entry.buffer.getSize() >= minSize) {
            entry.inUse = true;
            return &entry.buffer;
        }
    }

    // No suitable buffer found, create a new one
    VkDeviceSize bufferSize = std::max(minSize, defaultBufferSize_);

    PoolEntry newEntry;
    newEntry.buffer.init(allocator_, bufferSize);
    newEntry.inUse = true;

    pool_.push_back(std::move(newEntry));

    spdlog::info("[StagingBufferPool] Created new staging buffer of size: {} MB (pool size: {})",
                 bufferSize / (1024 * 1024), pool_.size());

    return &pool_.back().buffer;
}

void StagingBufferPool::release(StagingBuffer* buffer) {
    for (auto& entry : pool_) {
        if (&entry.buffer == buffer) {
            entry.inUse = false;
            return;
        }
    }

    spdlog::warn("[StagingBufferPool] Warning: Tried to release buffer not in pool!");
}

void StagingBufferPool::reset() {
    // Mark all buffers as available
    for (auto& entry : pool_) {
        entry.inUse = false;
    }
}

} // namespace FarHorizon