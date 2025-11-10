#pragma once

#include "core/VulkanContext.hpp"
#include "swapchain/Swapchain.hpp"
#include "sync/FrameSync.hpp"
#include "command/CommandPool.hpp"
#include "command/CommandBuffer.hpp"
#include "memory/StagingBufferPool.hpp"
#include "memory/RingBuffer.hpp"
#include <vector>

namespace FarHorizon {

// High-level rendering interface
class RenderContext {
public:
    RenderContext() = default;
    ~RenderContext() = default;

    // No copy
    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;

    // Move semantics
    RenderContext(RenderContext&& other) noexcept = default;
    RenderContext& operator=(RenderContext&& other) noexcept = default;

    void init(VulkanContext& context, Swapchain& swapchain);
    void shutdown();

    // Frame lifecycle
    bool beginFrame();  // Returns false if swapchain needs recreation
    void endFrame();

    // Get current frame's command buffer
    CommandBuffer getCurrentCommandBuffer() const;

    // Getters
    VulkanContext& getContext() { return *m_context; }
    Swapchain& getSwapchain() { return *m_swapchain; }
    uint32_t getCurrentImageIndex() const { return m_currentImageIndex; }
    StagingBufferPool& getStagingPool() { return m_stagingPool; }
    RingBuffer& getCurrentRingBuffer() { return m_ringBuffers[m_frameSync.getCurrentFrameIndex()]; }

private:
    VulkanContext* m_context = nullptr;
    Swapchain* m_swapchain = nullptr;
    FrameSync m_frameSync;

    // Per-frame command pools (one per frame in flight)
    std::vector<CommandPool> m_commandPools;
    std::vector<std::vector<VkCommandBuffer>> m_commandBuffers;

    // Per-swapchain-image renderFinished semaphores (indexed by swapchain image index)
    // Using per-image semaphores prevents reuse issues with triple buffering
    std::vector<Semaphore> m_renderFinishedSemaphores;

    // Memory management
    StagingBufferPool m_stagingPool;
    std::vector<RingBuffer> m_ringBuffers; // One per frame in flight

    uint32_t m_currentImageIndex = 0;
    bool m_frameInProgress = false;
};

} // namespace FarHorizon