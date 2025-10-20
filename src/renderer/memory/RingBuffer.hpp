#pragma once

#include "Buffer.hpp"

namespace VoxelEngine {

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
    VkBuffer getBuffer() const { return m_buffer.getBuffer(); }
    VkDeviceSize getSize() const { return m_buffer.getSize(); }
    VkDeviceSize getCurrentOffset() const { return m_currentOffset; }
    VkDeviceAddress getDeviceAddress() const { return m_buffer.getDeviceAddress(); }

private:
    Buffer m_buffer;
    VkDeviceSize m_currentOffset = 0;
    VkDeviceSize m_alignment = 256;

    // Helper to align offset
    VkDeviceSize alignOffset(VkDeviceSize offset) const {
        return (offset + m_alignment - 1) & ~(m_alignment - 1);
    }
};

} // namespace VoxelEngine