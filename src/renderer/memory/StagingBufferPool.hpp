#pragma once

#include "StagingBuffer.hpp"
#include <vector>
#include <memory>

namespace FarHorizon {

// Pool of staging buffers for efficient reuse
// Staging buffers are expensive to create, so we pool them
class StagingBufferPool {
public:
    StagingBufferPool() = default;
    ~StagingBufferPool() { cleanup(); }

    // No copy
    StagingBufferPool(const StagingBufferPool&) = delete;
    StagingBufferPool& operator=(const StagingBufferPool&) = delete;

    // Move semantics
    StagingBufferPool(StagingBufferPool&& other) noexcept = default;
    StagingBufferPool& operator=(StagingBufferPool&& other) noexcept = default;

    void init(VmaAllocator allocator, VkDeviceSize defaultBufferSize = 64 * 1024 * 1024); // 64MB default
    void cleanup();

    // Acquire a staging buffer of at least the specified size
    // Returns nullptr if allocation fails
    StagingBuffer* acquire(VkDeviceSize minSize);

    // Release a staging buffer back to the pool
    void release(StagingBuffer* buffer);

    // Reset pool (call after frame submission to make all buffers available again)
    void reset();

private:
    struct PoolEntry {
        StagingBuffer buffer;
        bool inUse = false;
    };

    VmaAllocator allocator_ = VK_NULL_HANDLE;
    VkDeviceSize defaultBufferSize_ = 0;
    std::vector<PoolEntry> pool_;
};

} // namespace FarHorizon