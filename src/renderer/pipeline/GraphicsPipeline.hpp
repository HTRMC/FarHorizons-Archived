#pragma once

#include "Shader.hpp"
#include <vulkan/vulkan.h>
#include <vector>

namespace FarHorizon {

// Graphics pipeline configuration
struct GraphicsPipelineConfig {
    // Shaders
    Shader* vertexShader = nullptr;
    Shader* fragmentShader = nullptr;

    // Vertex input (empty for hardcoded triangle)
    std::vector<VkVertexInputBindingDescription> vertexBindings;
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;

    // Descriptor set layouts (for bindless textures, etc.)
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

    // Push constant ranges
    std::vector<VkPushConstantRange> pushConstantRanges;

    // Color format
    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;

    // Viewport/scissor (dynamic)
    bool dynamicViewport = true;
    bool dynamicScissor = true;

    // Depth test
    bool depthTest = false;
    bool depthWrite = false;

    // Culling
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // Blending (disabled for opaque)
    bool blendEnable = false;

    // Custom blend factors (optional - used when blendEnable = true)
    // Defaults to normal alpha blending if not overridden
    VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    VkBlendOp colorBlendOp = VK_BLEND_OP_ADD;
    VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD;

    // Topology (default to triangle list)
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Line width (for line primitives)
    float lineWidth = 1.0f;
};

// Graphics pipeline wrapper
class GraphicsPipeline {
public:
    GraphicsPipeline() = default;
    ~GraphicsPipeline() { cleanup(); }

    // No copy
    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

    // Move semantics
    GraphicsPipeline(GraphicsPipeline&& other) noexcept;
    GraphicsPipeline& operator=(GraphicsPipeline&& other) noexcept;

    void init(VkDevice device, const GraphicsPipelineConfig& config);
    void cleanup();

    VkPipeline getPipeline() const { return m_pipeline; }
    VkPipelineLayout getLayout() const { return m_layout; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
};

} // namespace FarHorizon