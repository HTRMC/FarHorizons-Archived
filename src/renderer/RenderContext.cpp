#include "RenderContext.hpp"
#include "core/VulkanDebug.hpp"
#include <iostream>

namespace VoxelEngine {

void RenderContext::init(VulkanContext& context, Swapchain& swapchain) {
    m_context = &context;
    m_swapchain = &swapchain;

    auto device = m_context->getDevice().getLogicalDevice();
    auto graphicsQueueFamily = m_context->getDevice().getQueueFamilyIndices().graphicsFamily.value();

    // Initialize frame sync
    m_frameSync.init(device);

    // Create command pools (one per frame in flight)
    m_commandPools.resize(FrameSync::MAX_FRAMES_IN_FLIGHT);
    m_commandBuffers.resize(FrameSync::MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < FrameSync::MAX_FRAMES_IN_FLIGHT; i++) {
        // Command pool with RESET flag (allows individual command buffer reset)
        m_commandPools[i].init(device, graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

        // Allocate one command buffer per frame
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPools[i].getPool();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        m_commandBuffers[i].resize(1);
        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, m_commandBuffers[i].data()));
    }

    // Create per-swapchain-image renderFinished semaphores (indexed by swapchain image index)
    // This ensures each swapchain image has a dedicated renderFinished semaphore,
    // preventing reuse issues when we have more swapchain images than frames in flight
    uint32_t imageCount = static_cast<uint32_t>(m_swapchain->getImages().size());
    m_renderFinishedSemaphores.resize(imageCount);

    for (uint32_t i = 0; i < imageCount; i++) {
        m_renderFinishedSemaphores[i].init(device);
    }

    std::cout << "[RenderContext] Initialized (Frames in flight: " << FrameSync::MAX_FRAMES_IN_FLIGHT
              << ", Swapchain images: " << imageCount << ")" << std::endl;
}

void RenderContext::shutdown() {
    if (m_context) {
        m_context->waitIdle();

        for (auto& pool : m_commandPools) {
            pool.cleanup();
        }
        m_commandPools.clear();
        m_commandBuffers.clear();

        m_frameSync.shutdown();

        std::cout << "[RenderContext] Shutdown" << std::endl;
    }

    m_context = nullptr;
    m_swapchain = nullptr;
}

bool RenderContext::beginFrame() {
    auto device = m_context->getDevice().getLogicalDevice();
    auto& frame = m_frameSync.getCurrentFrame();

    // Wait for previous frame to finish (CPU-GPU sync)
    frame.renderFence.wait();
    frame.renderFence.reset();

    // Acquire next swapchain image
    // Use per-frame imageAvailable semaphore (doesn't matter which image we get)
    VkResult result = m_swapchain->acquireNextImage(
        frame.imageAvailableSemaphore.getSemaphore(),
        m_currentImageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Swapchain needs recreation
        return false;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cerr << "[RenderContext] Failed to acquire swapchain image!" << std::endl;
        assert(false);
    }

    m_frameInProgress = true;

    // Begin command buffer
    auto cmd = getCurrentCommandBuffer();
    cmd.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // Transition swapchain image to color attachment
    cmd.transitionImageLayout(
        m_swapchain->getImages()[m_currentImageIndex],
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );

    return true;
}

void RenderContext::endFrame() {
    if (!m_frameInProgress) {
        return;
    }

    auto device = m_context->getDevice().getLogicalDevice();
    auto& frame = m_frameSync.getCurrentFrame();
    auto cmd = getCurrentCommandBuffer();

    // Transition swapchain image to present
    cmd.transitionImageLayout(
        m_swapchain->getImages()[m_currentImageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    );

    cmd.end();

    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // Wait on per-frame imageAvailable semaphore
    VkSemaphore waitSemaphores[] = {frame.imageAvailableSemaphore.getSemaphore()};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    VkCommandBuffer cmdBuffer = cmd.getBuffer();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    // Signal per-image renderFinished semaphore (prevents reuse issues with triple buffering)
    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentImageIndex].getSemaphore()};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VK_CHECK(vkQueueSubmit(m_context->getDevice().getGraphicsQueue(), 1, &submitInfo, frame.renderFence.getFence()));

    // Present, waiting on per-image renderFinished semaphore
    VkResult result = m_swapchain->present(
        m_context->getDevice().getPresentQueue(),
        m_renderFinishedSemaphores[m_currentImageIndex].getSemaphore(),
        m_currentImageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // Swapchain needs recreation (handle in main loop)
    } else if (result != VK_SUCCESS) {
        std::cerr << "[RenderContext] Failed to present swapchain image!" << std::endl;
        assert(false);
    }

    // Advance to next frame
    m_frameSync.nextFrame();
    m_frameInProgress = false;
}

CommandBuffer RenderContext::getCurrentCommandBuffer() const {
    uint32_t frameIndex = m_frameSync.getCurrentFrameIndex();
    return CommandBuffer(m_commandBuffers[frameIndex][0]);
}

} // namespace VoxelEngine