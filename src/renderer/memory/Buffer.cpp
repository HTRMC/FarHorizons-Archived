#include "Buffer.hpp"
#include "../core/VulkanDebug.hpp"
#include <iostream>
#include <cstring>

namespace VoxelEngine {

Buffer::Buffer(Buffer&& other) noexcept
    : m_allocator(other.m_allocator)
    , m_buffer(other.m_buffer)
    , m_allocation(other.m_allocation)
    , m_size(other.m_size)
    , m_deviceAddress(other.m_deviceAddress)
    , m_mappedData(other.m_mappedData)
{
    other.m_allocator = VK_NULL_HANDLE;
    other.m_buffer = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
    other.m_size = 0;
    other.m_deviceAddress = 0;
    other.m_mappedData = nullptr;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        cleanup();

        m_allocator = other.m_allocator;
        m_buffer = other.m_buffer;
        m_allocation = other.m_allocation;
        m_size = other.m_size;
        m_deviceAddress = other.m_deviceAddress;
        m_mappedData = other.m_mappedData;

        other.m_allocator = VK_NULL_HANDLE;
        other.m_buffer = VK_NULL_HANDLE;
        other.m_allocation = VK_NULL_HANDLE;
        other.m_size = 0;
        other.m_deviceAddress = 0;
        other.m_mappedData = nullptr;
    }
    return *this;
}

void Buffer::init(
    VmaAllocator allocator,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VmaMemoryUsage memoryUsage,
    VmaAllocationCreateFlags flags
) {
    m_allocator = allocator;
    m_size = size;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;
    allocInfo.flags = flags;

    VK_CHECK(vmaCreateBuffer(
        m_allocator,
        &bufferInfo,
        &allocInfo,
        &m_buffer,
        &m_allocation,
        nullptr
    ));

    // Get device address if buffer supports it
    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        VkBufferDeviceAddressInfo addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = m_buffer;

        VmaAllocatorInfo allocatorInfo;
        vmaGetAllocatorInfo(m_allocator, &allocatorInfo);
        m_deviceAddress = vkGetBufferDeviceAddress(allocatorInfo.device, &addressInfo);
    }
}

void Buffer::cleanup() {
    if (m_buffer != VK_NULL_HANDLE) {
        if (m_mappedData != nullptr) {
            unmap();
        }
        vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
        m_size = 0;
        m_deviceAddress = 0;
    }
}

void* Buffer::map() {
    if (m_mappedData == nullptr) {
        VK_CHECK(vmaMapMemory(m_allocator, m_allocation, &m_mappedData));
    }
    return m_mappedData;
}

void Buffer::unmap() {
    if (m_mappedData != nullptr) {
        vmaUnmapMemory(m_allocator, m_allocation);
        m_mappedData = nullptr;
    }
}

void Buffer::copyData(const void* data, size_t size, size_t offset) {
    if (m_mappedData == nullptr) {
        std::cerr << "[Buffer] Cannot copy data to unmapped buffer!" << std::endl;
        assert(false);
    }

    if (offset + size > m_size) {
        std::cerr << "[Buffer] Copy size exceeds buffer bounds!" << std::endl;
        assert(false);
    }

    std::memcpy(static_cast<char*>(m_mappedData) + offset, data, size);
}

} // namespace VoxelEngine