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
    VkBuffer getBuffer() const { return m_buffer; }
    VmaAllocation getAllocation() const { return m_allocation; }
    VkDeviceSize getSize() const { return m_size; }
    VkDeviceAddress getDeviceAddress() const { return m_deviceAddress; }

private:
    VmaAllocator m_allocator = VK_NULL_HANDLE;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
    VkDeviceAddress m_deviceAddress = 0;
    void* m_mappedData = nullptr;
};

} // namespace FarHorizon