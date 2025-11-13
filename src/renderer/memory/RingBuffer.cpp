#include "RingBuffer.hpp"
#include "../core/VulkanDebug.hpp"
#include <spdlog/spdlog.h>

namespace FarHorizon {

void RingBuffer::init(VmaAllocator allocator, VkDeviceSize size, VkDeviceSize alignment) {
    alignment_ = alignment;

    // Create persistently mapped buffer for dynamic data
    buffer_.init(
        allocator,
        size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    // Keep it persistently mapped
    buffer_.map();

    spdlog::info("[RingBuffer] Initialized with size: {} KB, alignment: {}",
                 size / 1024, alignment);
}

void RingBuffer::cleanup() {
    buffer_.cleanup();
}

VkDeviceSize RingBuffer::allocate(VkDeviceSize size) {
    // Align current offset
    VkDeviceSize alignedOffset = alignOffset(currentOffset_);

    // Check if we have enough space
    if (alignedOffset + size > buffer_.getSize()) {
        spdlog::error("[RingBuffer] Out of space! Requested: {} bytes, available: {} bytes",
                      size, buffer_.getSize() - alignedOffset);
        return VK_WHOLE_SIZE;
    }

    // Update current offset for next allocation
    currentOffset_ = alignedOffset + size;

    return alignedOffset;
}

void RingBuffer::write(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    buffer_.copyData(data, size, offset);
}

void RingBuffer::reset() {
    currentOffset_ = 0;
}

} // namespace FarHorizon