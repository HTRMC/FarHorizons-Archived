#pragma once

#include "Buffer.hpp"
#include "../command/CommandBuffer.hpp"

namespace FarHorizon {

// Helper for uploading data from CPU to GPU via staging buffer
class StagingBuffer {
public:
    StagingBuffer() = default;
    ~StagingBuffer() = default;

    // No copy
    StagingBuffer(const StagingBuffer&) = delete;
    StagingBuffer& operator=(const StagingBuffer&) = delete;

    // Move semantics
    StagingBuffer(StagingBuffer&& other) noexcept = default;
    StagingBuffer& operator=(StagingBuffer&& other) noexcept = default;

    // Initialize staging buffer
    void init(VmaAllocator allocator, VkDeviceSize size);
    void cleanup();

    // Upload data to destination buffer
    // Returns false if staging buffer is too small
    bool upload(
        CommandBuffer cmd,
        const void* data,
        VkDeviceSize size,
        Buffer& dstBuffer,
        VkDeviceSize dstOffset = 0
    );

    // Getters
    VkDeviceSize getSize() const { return m_stagingBuffer.getSize(); }
    bool isValid() const { return m_stagingBuffer.getBuffer() != VK_NULL_HANDLE; }

private:
    Buffer m_stagingBuffer;
};

} // namespace FarHorizon