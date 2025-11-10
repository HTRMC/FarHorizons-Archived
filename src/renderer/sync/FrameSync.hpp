#pragma once

#include "Fence.hpp"
#include "Semaphore.hpp"
#include <vulkan/vulkan.h>
#include <vector>

namespace FarHorizon {

// Per-frame synchronization data
struct FrameData {
    Fence renderFence;               // CPU-GPU sync (wait for previous frame to finish)
    Semaphore imageAvailableSemaphore; // GPU-GPU sync (swapchain image ready)
    Semaphore renderFinishedSemaphore; // GPU-GPU sync (rendering finished)
};

// Manages frame-in-flight synchronization (double/triple buffering)
class FrameSync {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2; // Double buffering

    FrameSync() = default;
    ~FrameSync() = default;

    // No copy
    FrameSync(const FrameSync&) = delete;
    FrameSync& operator=(const FrameSync&) = delete;

    // Move semantics
    FrameSync(FrameSync&& other) noexcept = default;
    FrameSync& operator=(FrameSync&& other) noexcept = default;

    void init(VkDevice device);
    void shutdown();

    // Get current frame data
    FrameData& getCurrentFrame() { return m_frames[m_currentFrame]; }
    const FrameData& getCurrentFrame() const { return m_frames[m_currentFrame]; }

    // Advance to next frame
    void nextFrame() { m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT; }

    uint32_t getCurrentFrameIndex() const { return m_currentFrame; }
    uint32_t getMaxFramesInFlight() const { return MAX_FRAMES_IN_FLIGHT; }

private:
    std::vector<FrameData> m_frames;
    uint32_t m_currentFrame = 0;
};

} // namespace FarHorizon