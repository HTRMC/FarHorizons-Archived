#include "StagingBuffer.hpp"
#include "../core/VulkanDebug.hpp"
#include <spdlog/spdlog.h>

namespace VoxelEngine {

void StagingBuffer::init(VmaAllocator allocator, VkDeviceSize size) {
    // Create staging buffer (HOST_VISIBLE for CPU writes, TRANSFER_SRC for GPU copies)
    m_stagingBuffer.init(
        allocator,
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    // Keep it persistently mapped for performance
    m_stagingBuffer.map();
}

void StagingBuffer::cleanup() {
    m_stagingBuffer.cleanup();
}

bool StagingBuffer::upload(
    CommandBuffer cmd,
    const void* data,
    VkDeviceSize size,
    Buffer& dstBuffer,
    VkDeviceSize dstOffset
) {
    if (size > m_stagingBuffer.getSize()) {
        spdlog::error("[StagingBuffer] Upload size ({}) exceeds staging buffer capacity ({})",
                      size, m_stagingBuffer.getSize());
        return false;
    }

    // Copy data to staging buffer
    m_stagingBuffer.copyData(data, size);

    // Record copy command
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size = size;

    vkCmdCopyBuffer(
        cmd.getBuffer(),
        m_stagingBuffer.getBuffer(),
        dstBuffer.getBuffer(),
        1,
        &copyRegion
    );

    return true;
}

} // namespace VoxelEngine