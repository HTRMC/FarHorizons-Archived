#include "CommandBuffer.hpp"
#include "../core/VulkanDebug.hpp"

namespace FarHorizon {

void CommandBuffer::begin(VkCommandBufferUsageFlags flags) const {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = flags;
    beginInfo.pInheritanceInfo = nullptr;

    VK_CHECK(vkBeginCommandBuffer(m_buffer, &beginInfo));
}

void CommandBuffer::end() const {
    VK_CHECK(vkEndCommandBuffer(m_buffer));
}

void CommandBuffer::beginRendering(
    VkImageView colorAttachment,
    VkExtent2D extent,
    const glm::vec4& clearColor,
    VkImageView depthAttachment
) const {
    // Color attachment
    VkRenderingAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachmentInfo.imageView = colorAttachment;
    colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentInfo.clearValue.color = {{clearColor.r, clearColor.g, clearColor.b, clearColor.a}};

    // Depth attachment (if provided)
    VkRenderingAttachmentInfo depthAttachmentInfo{};
    if (depthAttachment != VK_NULL_HANDLE) {
        depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachmentInfo.imageView = depthAttachment;
        depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachmentInfo.clearValue.depthStencil = {1.0f, 0};
    }

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = extent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachmentInfo;
    renderingInfo.pDepthAttachment = (depthAttachment != VK_NULL_HANDLE) ? &depthAttachmentInfo : nullptr;
    renderingInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(m_buffer, &renderingInfo);
}

void CommandBuffer::endRendering() const {
    vkCmdEndRendering(m_buffer);
}

void CommandBuffer::bindPipeline(VkPipeline pipeline, VkPipelineBindPoint bindPoint) const {
    vkCmdBindPipeline(m_buffer, bindPoint, pipeline);
}

void CommandBuffer::bindDescriptorSets(
    VkPipelineLayout layout,
    uint32_t firstSet,
    uint32_t descriptorSetCount,
    const VkDescriptorSet* descriptorSets,
    VkPipelineBindPoint bindPoint
) const {
    vkCmdBindDescriptorSets(m_buffer, bindPoint, layout, firstSet, descriptorSetCount, descriptorSets, 0, nullptr);
}

void CommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) const {
    vkCmdDraw(m_buffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) const {
    vkCmdDrawIndexed(m_buffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandBuffer::drawIndexedIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) const {
    vkCmdDrawIndexedIndirect(m_buffer, buffer, offset, drawCount, stride);
}

void CommandBuffer::bindVertexBuffer(VkBuffer buffer, VkDeviceSize offset) const {
    VkBuffer buffers[] = {buffer};
    VkDeviceSize offsets[] = {offset};
    vkCmdBindVertexBuffers(m_buffer, 0, 1, buffers, offsets);
}

void CommandBuffer::bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) const {
    vkCmdBindIndexBuffer(m_buffer, buffer, offset, indexType);
}

void CommandBuffer::pushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* data) const {
    vkCmdPushConstants(m_buffer, layout, stageFlags, offset, size, data);
}

void CommandBuffer::setViewport(const VkViewport& viewport) const {
    vkCmdSetViewport(m_buffer, 0, 1, &viewport);
}

void CommandBuffer::setScissor(const VkRect2D& scissor) const {
    vkCmdSetScissor(m_buffer, 0, 1, &scissor);
}

void CommandBuffer::transitionImageLayout(
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkImageAspectFlags aspectMask
) const {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(m_buffer, &dependencyInfo);
}

} // namespace FarHorizon