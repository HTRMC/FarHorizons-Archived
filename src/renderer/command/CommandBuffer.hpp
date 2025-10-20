#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace VoxelEngine {

// Lightweight command buffer wrapper
class CommandBuffer {
public:
    CommandBuffer() = default;
    CommandBuffer(VkCommandBuffer buffer) : m_buffer(buffer) {}

    // Begin/End recording
    void begin(VkCommandBufferUsageFlags flags = 0) const;
    void end() const;

    // Dynamic rendering (Vulkan 1.3)
    void beginRendering(
        VkImageView colorAttachment,
        VkExtent2D extent,
        const glm::vec4& clearColor = {0.0f, 0.0f, 0.0f, 1.0f},
        VkImageView depthAttachment = VK_NULL_HANDLE
    ) const;
    void endRendering() const;

    // Pipeline binding
    void bindPipeline(VkPipeline pipeline, VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS) const;
    void bindDescriptorSets(
        VkPipelineLayout layout,
        uint32_t firstSet,
        uint32_t descriptorSetCount,
        const VkDescriptorSet* descriptorSets,
        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS
    ) const;

    // Drawing
    void draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0) const;
    void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0) const;

    // Vertex/Index buffers
    void bindVertexBuffer(VkBuffer buffer, VkDeviceSize offset = 0) const;
    void bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset = 0, VkIndexType indexType = VK_INDEX_TYPE_UINT32) const;

    // Viewport/Scissor
    void setViewport(const VkViewport& viewport) const;
    void setScissor(const VkRect2D& scissor) const;

    // Push constants
    void pushConstants(
        VkPipelineLayout layout,
        VkShaderStageFlags stageFlags,
        uint32_t offset,
        uint32_t size,
        const void* data
    ) const;

    // Image transitions
    void transitionImageLayout(
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
    ) const;

    VkCommandBuffer getBuffer() const { return m_buffer; }
    operator VkCommandBuffer() const { return m_buffer; }

private:
    VkCommandBuffer m_buffer = VK_NULL_HANDLE;
};

} // namespace VoxelEngine