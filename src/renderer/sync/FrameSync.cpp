#include "FrameSync.hpp"
#include <iostream>

namespace VoxelEngine {

void FrameSync::init(VkDevice device) {
    m_frames.resize(MAX_FRAMES_IN_FLIGHT);

    for (auto& frame : m_frames) {
        frame.renderFence.init(device, true); // Start signaled (first frame doesn't wait)
        frame.imageAvailableSemaphore.init(device);
        frame.renderFinishedSemaphore.init(device);
    }

    std::cout << "[FrameSync] Initialized with " << MAX_FRAMES_IN_FLIGHT << " frames in flight" << std::endl;
}

void FrameSync::shutdown() {
    for (auto& frame : m_frames) {
        frame.renderFence.cleanup();
        frame.imageAvailableSemaphore.cleanup();
        frame.renderFinishedSemaphore.cleanup();
    }
    m_frames.clear();
    m_currentFrame = 0;

    std::cout << "[FrameSync] Shutdown" << std::endl;
}

} // namespace VoxelEngine