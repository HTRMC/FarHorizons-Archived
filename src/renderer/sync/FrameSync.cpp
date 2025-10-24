#include "FrameSync.hpp"
#include <spdlog/spdlog.h>

namespace VoxelEngine {

void FrameSync::init(VkDevice device) {
    m_frames.resize(MAX_FRAMES_IN_FLIGHT);

    for (auto& frame : m_frames) {
        frame.renderFence.init(device, true); // Start signaled (first frame doesn't wait)
        frame.imageAvailableSemaphore.init(device);
        frame.renderFinishedSemaphore.init(device);
    }

    spdlog::info("[FrameSync] Initialized with {} frames in flight", MAX_FRAMES_IN_FLIGHT);
}

void FrameSync::shutdown() {
    for (auto& frame : m_frames) {
        frame.renderFence.cleanup();
        frame.imageAvailableSemaphore.cleanup();
        frame.renderFinishedSemaphore.cleanup();
    }
    m_frames.clear();
    m_currentFrame = 0;

    spdlog::info("[FrameSync] Shutdown");
}

} // namespace VoxelEngine