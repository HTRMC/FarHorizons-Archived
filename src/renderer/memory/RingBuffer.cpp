#include "RingBuffer.hpp"
#include "../core/VulkanDebug.hpp"
#include <spdlog/spdlog.h>

namespace FarHorizon {

void RingBuffer::init(VmaAllocator allocator, VkDeviceSize size, VkDeviceSize alignment) {
    m_alignment = alignment;

    // Create persistently mapped buffer for dynamic data
    m_buffer.init(
        allocator,
        size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    // Keep it persistently mapped
    m_buffer.map();

    spdlog::info("[RingBuffer] Initialized with size: {} KB, alignment: {}",
                 size / 1024, alignment);
}

void RingBuffer::cleanup() {
    m_buffer.cleanup();
}

VkDeviceSize RingBuffer::allocate(VkDeviceSize size) {
    // Align current offset
    VkDeviceSize alignedOffset = alignOffset(m_currentOffset);

    // Check if we have enough space
    if (alignedOffset + size > m_buffer.getSize()) {
        spdlog::error("[RingBuffer] Out of space! Requested: {} bytes, available: {} bytes",
                      size, m_buffer.getSize() - alignedOffset);
        return VK_WHOLE_SIZE;
    }

    // Update current offset for next allocation
    m_currentOffset = alignedOffset + size;

    return alignedOffset;
}

void RingBuffer::write(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    m_buffer.copyData(data, size, offset);
}

void RingBuffer::reset() {
    m_currentOffset = 0;
}

} // namespace FarHorizon