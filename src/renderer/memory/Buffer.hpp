#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <cstddef>

namespace FarHorizon {

// RAII wrapper for VMA-allocated buffers
class Buffer {
public:
    Buffer() = default;
    ~Buffer() { cleanup(); }

    // No copy
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // Move semantics
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    // Create buffer with VMA
    void init(
        VmaAllocator allocator,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VmaMemoryUsage memoryUsage,
        VmaAllocationCreateFlags flags = 0
    );

    void cleanup();

    // Map/unmap memory (only valid for HOST_VISIBLE allocations)
    void* map();
    void unmap();

    // Copy data to buffer (convenience for mapped buffers)
    void copyData(const void* data, size_t size, size_t offset = 0);

    // Getters
    VkBuffer getBuffer() const { return buffer_; }
    VmaAllocation getAllocation() const { return allocation_; }
    VkDeviceSize getSize() const { return size_; }
    VkDeviceAddress getDeviceAddress() const { return deviceAddress_; }

private:
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;
    VkDeviceSize size_ = 0;
    VkDeviceAddress deviceAddress_ = 0;
    void* mappedData_ = nullptr;
};

} // namespace FarHorizon