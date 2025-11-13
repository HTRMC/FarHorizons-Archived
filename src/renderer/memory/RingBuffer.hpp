#pragma once

#include "Buffer.hpp"

namespace FarHorizon {

// Ring buffer for per-frame dynamic data (e.g., uniforms, push constants data)
// Allocates from a circular buffer, resetting when wrapping around
// Thread-safe for single producer (render thread)
class RingBuffer {
public:
    RingBuffer() = default;
    ~RingBuffer() { cleanup(); }

    // No copy
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    // Move semantics
    RingBuffer(RingBuffer&& other) noexcept = default;
    RingBuffer& operator=(RingBuffer&& other) noexcept = default;

    // Initialize ring buffer
    // size: Total size of the ring buffer
    // alignment: Minimum alignment for allocations (typically 256 for uniform buffers)
    void init(VmaAllocator allocator, VkDeviceSize size, VkDeviceSize alignment = 256);
    void cleanup();

    // Allocate from ring buffer
    // Returns offset into the buffer, or VK_WHOLE_SIZE if allocation fails
    VkDeviceSize allocate(VkDeviceSize size);

    // Write data to ring buffer at the given offset
    void write(const void* data, VkDeviceSize size, VkDeviceSize offset);

    // Reset ring buffer (call at beginning of frame)
    void reset();

    // Getters
    VkBuffer getBuffer() const { return buffer_.getBuffer(); }
    VkDeviceSize getSize() const { return buffer_.getSize(); }
    VkDeviceSize getCurrentOffset() const { return currentOffset_; }
    VkDeviceAddress getDeviceAddress() const { return buffer_.getDeviceAddress(); }

private:
    Buffer buffer_;
    VkDeviceSize currentOffset_ = 0;
    VkDeviceSize alignment_ = 256;

    // Helper to align offset
    VkDeviceSize alignOffset(VkDeviceSize offset) const {
        return (offset + alignment_ - 1) & ~(alignment_ - 1);
    }
};

} // namespace FarHorizon