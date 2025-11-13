#include "Buffer.hpp"
#include "../core/VulkanDebug.hpp"
#include <spdlog/spdlog.h>
#include <cstring>

namespace FarHorizon {

Buffer::Buffer(Buffer&& other) noexcept
    : allocator_(other.allocator_)
    , buffer_(other.buffer_)
    , allocation_(other.allocation_)
    , size_(other.size_)
    , deviceAddress_(other.deviceAddress_)
    , mappedData_(other.mappedData_)
{
    other.allocator_ = VK_NULL_HANDLE;
    other.buffer_ = VK_NULL_HANDLE;
    other.allocation_ = VK_NULL_HANDLE;
    other.size_ = 0;
    other.deviceAddress_ = 0;
    other.mappedData_ = nullptr;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        cleanup();

        allocator_ = other.allocator_;
        buffer_ = other.buffer_;
        allocation_ = other.allocation_;
        size_ = other.size_;
        deviceAddress_ = other.deviceAddress_;
        mappedData_ = other.mappedData_;

        other.allocator_ = VK_NULL_HANDLE;
        other.buffer_ = VK_NULL_HANDLE;
        other.allocation_ = VK_NULL_HANDLE;
        other.size_ = 0;
        other.deviceAddress_ = 0;
        other.mappedData_ = nullptr;
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
    allocator_ = allocator;
    size_ = size;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;
    allocInfo.flags = flags;

    VK_CHECK(vmaCreateBuffer(
        allocator_,
        &bufferInfo,
        &allocInfo,
        &buffer_,
        &allocation_,
        nullptr
    ));

    // Get device address if buffer supports it
    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        VkBufferDeviceAddressInfo addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = buffer_;

        VmaAllocatorInfo allocatorInfo;
        vmaGetAllocatorInfo(allocator_, &allocatorInfo);
        deviceAddress_ = vkGetBufferDeviceAddress(allocatorInfo.device, &addressInfo);
    }
}

void Buffer::cleanup() {
    if (buffer_ != VK_NULL_HANDLE) {
        if (mappedData_ != nullptr) {
            unmap();
        }
        vmaDestroyBuffer(allocator_, buffer_, allocation_);
        buffer_ = VK_NULL_HANDLE;
        allocation_ = VK_NULL_HANDLE;
        size_ = 0;
        deviceAddress_ = 0;
    }
}

void* Buffer::map() {
    if (mappedData_ == nullptr) {
        VK_CHECK(vmaMapMemory(allocator_, allocation_, &mappedData_));
    }
    return mappedData_;
}

void Buffer::unmap() {
    if (mappedData_ != nullptr) {
        vmaUnmapMemory(allocator_, allocation_);
        mappedData_ = nullptr;
    }
}

void Buffer::copyData(const void* data, size_t size, size_t offset) {
    if (mappedData_ == nullptr) {
        spdlog::error("[Buffer] Cannot copy data to unmapped buffer!");
        assert(false);
    }

    if (offset + size > size_) {
        spdlog::error("[Buffer] Copy size exceeds buffer bounds!");
        assert(false);
    }

    std::memcpy(static_cast<char*>(mappedData_) + offset, data, size);
}

} // namespace FarHorizon