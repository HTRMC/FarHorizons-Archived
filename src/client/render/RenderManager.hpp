#pragma once

#include <memory>
#include <glm/glm.hpp>
#include "renderer/core/VulkanContext.hpp"
#include "renderer/swapchain/Swapchain.hpp"
#include "renderer/RenderContext.hpp"
#include "renderer/DepthBuffer.hpp"
#include "renderer/OffscreenTarget.hpp"
#include "renderer/pipeline/GraphicsPipeline.hpp"
#include "renderer/pipeline/Shader.hpp"
#include "renderer/memory/Buffer.hpp"
#include "renderer/memory/ChunkBufferManager.hpp"
#include "world/ChunkGpuData.hpp"
#include "game/GameStateManager.hpp"
#include "core/Camera.hpp"
#include "world/ChunkManager.hpp"
#include "core/Settings.hpp"
#include "core/Raycast.hpp"
#include "TextureManager.hpp"

namespace FarHorizon {

/**
 * Manages all rendering resources and pipelines.
 */
class RenderManager {
public:
    RenderManager();
    ~RenderManager();

    /**
     * Initialize all rendering resources
     */
    void init(GLFWwindow* window, TextureManager& textureManager);

    /**
     * Begin a new frame
     */
    bool beginFrame();

    /**
     * Render the entire frame
     */
    void render(Camera& camera, ChunkManager& chunkManager,
                GameStateManager& gameStateManager, Settings& settings,
                TextureManager& textureManager,
                const std::optional<BlockHitResult>& crosshairTarget);

    /**
     * End the current frame and present
     */
    void endFrame();

    /**
     * Handle window resize
     */
    void onResize(uint32_t width, uint32_t height, TextureManager& textureManager);

    /**
     * Wait for GPU to be idle
     */
    void waitIdle();

    /**
     * Get Vulkan allocator for buffer creation
     */
    VmaAllocator getAllocator() const { return vulkanContext->getAllocator(); }

    /**
     * Get logical device
     */
    VkDevice getDevice() const { return vulkanContext->getDevice().getLogicalDevice(); }

    /**
     * Get graphics queue
     */
    VkQueue getGraphicsQueue() const { return vulkanContext->getDevice().getGraphicsQueue(); }

    /**
     * Get queue family indices
     */
    const QueueFamilyIndices& getQueueFamilyIndices() const {
        return vulkanContext->getDevice().getQueueFamilyIndices();
    }

    /**
     * Get chunk buffer manager
     */
    ChunkBufferManager& getChunkBufferManager() { return *bufferManager; }

    /**
     * Mark QuadInfo buffer for update
     */
    void markQuadInfoForUpdate() { quadInfoNeedsUpdate = true; }

    /**
     * Clear all chunk mesh data from GPU buffers
     */
    void clearChunkBuffers();

    /**
     * Get window dimensions
     */
    uint32_t getWidth() const { return swapchain->getExtent().width; }
    uint32_t getHeight() const { return swapchain->getExtent().height; }

private:
    void createPipelines(TextureManager& textureManager);
    void createBuffers();

    struct PushConstants {
        glm::mat4 viewProj;
        glm::ivec3 cameraPositionInteger;
        float _pad0;
        glm::vec3 cameraPositionFraction;
        float _pad1;
    };

    void renderScene(Camera& camera, ChunkManager& chunkManager,
                     GameStateManager& gameStateManager,
                     const std::optional<BlockHitResult>& crosshairTarget,
                     TextureManager& textureManager, CommandBuffer& cmd, const VkViewport& viewport);
    void renderBlockOutline(const BlockHitResult& target, CommandBuffer& cmd,
                           const PushConstants& pushConstants);
    void renderUI(GameStateManager& gameStateManager, Settings& settings,
                  Camera& camera, bool needsBlur, GameStateManager::State currentState,
                  TextureManager& textureManager, CommandBuffer& cmd);
    void applyBlurPostProcessing(Settings& settings, GameStateManager& gameStateManager,
                                GameStateManager::State currentState, TextureManager& textureManager,
                                CommandBuffer& cmd, const VkViewport& viewport, const VkRect2D& scissor);

private:
    // Core Vulkan resources
    std::unique_ptr<VulkanContext> vulkanContext;
    std::unique_ptr<Swapchain> swapchain;
    std::unique_ptr<RenderContext> renderer;
    std::unique_ptr<DepthBuffer> depthBuffer;

    // Pipelines
    std::unique_ptr<GraphicsPipeline> mainPipeline;
    std::unique_ptr<GraphicsPipeline> textPipeline;
    std::unique_ptr<GraphicsPipeline> panelPipeline;
    std::unique_ptr<GraphicsPipeline> outlinePipeline;
    std::unique_ptr<GraphicsPipeline> blurPipeline;

    // Shaders
    std::unique_ptr<Shader> vertShader, fragShader;
    std::unique_ptr<Shader> textVertShader, textFragShader;
    std::unique_ptr<Shader> panelVertShader, panelFragShader;
    std::unique_ptr<Shader> outlineVertShader, outlineFragShader;
    std::unique_ptr<Shader> blurVertShader, blurFragShader;

    // Buffers
    std::unique_ptr<Buffer> quadInfoBuffer;
    std::unique_ptr<Buffer> textVertexBuffer;
    std::unique_ptr<Buffer> panelVertexBuffer;
    std::unique_ptr<Buffer> outlineVertexBuffer;
    std::unique_ptr<ChunkBufferManager> bufferManager;

    // Descriptor sets
    VkDescriptorSetLayout geometrySetLayout;
    VkDescriptorPool geometryDescriptorPool;
    VkDescriptorSet geometryDescriptorSet;

    // Post-processing
    std::unique_ptr<OffscreenTarget> sceneTarget;
    std::unique_ptr<OffscreenTarget> blurTarget1;
    uint32_t sceneTextureIndex;
    uint32_t blurTexture1Index;

    // State tracking
    bool quadInfoNeedsUpdate;
};

} // namespace FarHorizon