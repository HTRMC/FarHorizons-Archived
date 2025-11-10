#include "RenderManager.hpp"
#include "TextureManager.hpp"
#include "core/Camera.hpp"
#include "core/Settings.hpp"
#include "core/Raycast.hpp"
#include "world/ChunkManager.hpp"
#include "world/BlockRegistry.hpp"
#include "game/GameStateManager.hpp"
#include "text/TextRenderer.hpp"
#include "text/Text.hpp"
#include "ui/PauseMenu.hpp"
#include "ui/MainMenu.hpp"
#include "ui/OptionsMenu.hpp"
#include <spdlog/spdlog.h>

namespace FarHorizon {

RenderManager::RenderManager()
    : vulkanContext(std::make_unique<VulkanContext>())
    , swapchain(std::make_unique<Swapchain>())
    , renderer(std::make_unique<RenderContext>())
    , depthBuffer(std::make_unique<DepthBuffer>())
    , mainPipeline(std::make_unique<GraphicsPipeline>())
    , textPipeline(std::make_unique<GraphicsPipeline>())
    , panelPipeline(std::make_unique<GraphicsPipeline>())
    , outlinePipeline(std::make_unique<GraphicsPipeline>())
    , blurPipeline(std::make_unique<GraphicsPipeline>())
    , vertShader(std::make_unique<Shader>())
    , fragShader(std::make_unique<Shader>())
    , textVertShader(std::make_unique<Shader>())
    , textFragShader(std::make_unique<Shader>())
    , panelVertShader(std::make_unique<Shader>())
    , panelFragShader(std::make_unique<Shader>())
    , outlineVertShader(std::make_unique<Shader>())
    , outlineFragShader(std::make_unique<Shader>())
    , blurVertShader(std::make_unique<Shader>())
    , blurFragShader(std::make_unique<Shader>())
    , quadInfoBuffer(std::make_unique<Buffer>())
    , textVertexBuffer(std::make_unique<Buffer>())
    , panelVertexBuffer(std::make_unique<Buffer>())
    , outlineVertexBuffer(std::make_unique<Buffer>())
    , bufferManager(std::make_unique<ChunkBufferManager>())
    , sceneTarget(std::make_unique<OffscreenTarget>())
    , blurTarget1(std::make_unique<OffscreenTarget>())
    , geometrySetLayout(VK_NULL_HANDLE)
    , geometryDescriptorPool(VK_NULL_HANDLE)
    , geometryDescriptorSet(VK_NULL_HANDLE)
    , sceneTextureIndex(0)
    , blurTexture1Index(0)
    , quadInfoNeedsUpdate(true) {
}

RenderManager::~RenderManager() = default;

void RenderManager::init(GLFWwindow* window, TextureManager& textureManager) {
    spdlog::info("Initializing rendering system...");

    vulkanContext->init(window, "Far Horizon");

    uint32_t width, height;
    glfwGetFramebufferSize(window, reinterpret_cast<int*>(&width), reinterpret_cast<int*>(&height));

    swapchain->init(*vulkanContext, width, height);
    renderer->init(*vulkanContext, *swapchain);
    depthBuffer->init(vulkanContext->getAllocator(), vulkanContext->getDevice().getLogicalDevice(),
                     width, height);

    VkCommandPoolCreateInfo uploadPoolInfo{};
    uploadPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    uploadPoolInfo.queueFamilyIndex = vulkanContext->getDevice().getQueueFamilyIndices().graphicsFamily.value();
    uploadPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    VkCommandPool uploadPool;
    vkCreateCommandPool(vulkanContext->getDevice().getLogicalDevice(), &uploadPoolInfo, nullptr, &uploadPool);

    VkCommandBufferAllocateInfo uploadAllocInfo{};
    uploadAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    uploadAllocInfo.commandPool = uploadPool;
    uploadAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    uploadAllocInfo.commandBufferCount = 1;

    VkCommandBuffer uploadCmd;
    vkAllocateCommandBuffers(vulkanContext->getDevice().getLogicalDevice(), &uploadAllocInfo, &uploadCmd);

    VkCommandBufferBeginInfo uploadBeginInfo{};
    uploadBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    uploadBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(uploadCmd, &uploadBeginInfo);

    textureManager.init(vulkanContext->getDevice().getLogicalDevice(), vulkanContext->getAllocator(), uploadCmd);

    vkEndCommandBuffer(uploadCmd);

    VkSubmitInfo uploadSubmitInfo{};
    uploadSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    uploadSubmitInfo.commandBufferCount = 1;
    uploadSubmitInfo.pCommandBuffers = &uploadCmd;
    vkQueueSubmit(vulkanContext->getDevice().getGraphicsQueue(), 1, &uploadSubmitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(vulkanContext->getDevice().getGraphicsQueue());

    vkDestroyCommandPool(vulkanContext->getDevice().getLogicalDevice(), uploadPool, nullptr);

    createPipelines(textureManager);
    createBuffers();

    // Initialize offscreen targets for blur
    sceneTarget->init(vulkanContext->getAllocator(), vulkanContext->getDevice().getLogicalDevice(),
                     width, height, swapchain->getImageFormat(), depthBuffer->getFormat());
    blurTarget1->init(vulkanContext->getAllocator(), vulkanContext->getDevice().getLogicalDevice(),
                     width, height, swapchain->getImageFormat(), VK_FORMAT_UNDEFINED);

    // Register external textures for post-processing
    sceneTextureIndex = textureManager.registerExternalTexture(sceneTarget->getColorImageView());
    blurTexture1Index = textureManager.registerExternalTexture(blurTarget1->getColorImageView());

    // Initialize offscreen image layouts
    VkCommandPoolCreateInfo initPoolInfo{};
    initPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    initPoolInfo.queueFamilyIndex = vulkanContext->getDevice().getQueueFamilyIndices().graphicsFamily.value();
    initPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    VkCommandPool initPool;
    vkCreateCommandPool(vulkanContext->getDevice().getLogicalDevice(), &initPoolInfo, nullptr, &initPool);

    VkCommandBufferAllocateInfo initAllocInfo{};
    initAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    initAllocInfo.commandPool = initPool;
    initAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    initAllocInfo.commandBufferCount = 1;

    VkCommandBuffer initCmd;
    vkAllocateCommandBuffers(vulkanContext->getDevice().getLogicalDevice(), &initAllocInfo, &initCmd);

    VkCommandBufferBeginInfo initBeginInfo{};
    initBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    initBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(initCmd, &initBeginInfo);

    VkImageMemoryBarrier initBarriers[2]{};
    initBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    initBarriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    initBarriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    initBarriers[0].srcAccessMask = 0;
    initBarriers[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    initBarriers[0].image = sceneTarget->getColorImage();
    initBarriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    initBarriers[0].subresourceRange.baseMipLevel = 0;
    initBarriers[0].subresourceRange.levelCount = 1;
    initBarriers[0].subresourceRange.baseArrayLayer = 0;
    initBarriers[0].subresourceRange.layerCount = 1;

    initBarriers[1] = initBarriers[0];
    initBarriers[1].image = blurTarget1->getColorImage();

    vkCmdPipelineBarrier(initCmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 2, initBarriers);

    vkEndCommandBuffer(initCmd);

    VkSubmitInfo initSubmitInfo{};
    initSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    initSubmitInfo.commandBufferCount = 1;
    initSubmitInfo.pCommandBuffers = &initCmd;
    vkQueueSubmit(vulkanContext->getDevice().getGraphicsQueue(), 1, &initSubmitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(vulkanContext->getDevice().getGraphicsQueue());

    vkDestroyCommandPool(vulkanContext->getDevice().getLogicalDevice(), initPool, nullptr);

    spdlog::info("Rendering system initialized");
}

void RenderManager::createPipelines(TextureManager& textureManager) {
    VkDevice device = vulkanContext->getDevice().getLogicalDevice();

    // Load shaders
    vertShader->loadFromFile(device, "assets/minecraft/shaders/triangle.vsh.spv");
    fragShader->loadFromFile(device, "assets/minecraft/shaders/triangle.fsh.spv");
    textVertShader->loadFromFile(device, "assets/minecraft/shaders/text.vsh.spv");
    textFragShader->loadFromFile(device, "assets/minecraft/shaders/text.fsh.spv");
    panelVertShader->loadFromFile(device, "assets/minecraft/shaders/panel.vsh.spv");
    panelFragShader->loadFromFile(device, "assets/minecraft/shaders/panel.fsh.spv");
    outlineVertShader->loadFromFile(device, "assets/minecraft/shaders/outline.vsh.spv");
    outlineFragShader->loadFromFile(device, "assets/minecraft/shaders/outline.fsh.spv");
    blurVertShader->loadFromFile(device, "assets/minecraft/shaders/blur.vsh.spv");
    blurFragShader->loadFromFile(device, "assets/minecraft/shaders/blur.fsh.spv");

    // Create geometry descriptor set layout
    VkDescriptorSetLayoutBinding quadInfoBinding{};
    quadInfoBinding.binding = 0;
    quadInfoBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    quadInfoBinding.descriptorCount = 1;
    quadInfoBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding lightingBinding{};
    lightingBinding.binding = 1;
    lightingBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    lightingBinding.descriptorCount = 1;
    lightingBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding chunkDataBinding{};
    chunkDataBinding.binding = 2;
    chunkDataBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    chunkDataBinding.descriptorCount = 1;
    chunkDataBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding faceDataBinding{};
    faceDataBinding.binding = 3;
    faceDataBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    faceDataBinding.descriptorCount = 1;
    faceDataBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding geometryBindings[] = {quadInfoBinding, lightingBinding, chunkDataBinding, faceDataBinding};

    VkDescriptorSetLayoutCreateInfo geometryLayoutInfo{};
    geometryLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    geometryLayoutInfo.bindingCount = 4;
    geometryLayoutInfo.pBindings = geometryBindings;

    vkCreateDescriptorSetLayout(device, &geometryLayoutInfo, nullptr, &geometrySetLayout);

    // Main pipeline
    GraphicsPipelineConfig pipelineConfig;
    pipelineConfig.vertexShader = vertShader.get();
    pipelineConfig.fragmentShader = fragShader.get();
    pipelineConfig.colorFormat = swapchain->getImageFormat();
    pipelineConfig.depthFormat = depthBuffer->getFormat();
    pipelineConfig.depthTest = true;
    pipelineConfig.depthWrite = true;
    pipelineConfig.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineConfig.descriptorSetLayouts.push_back(textureManager.getDescriptorSetLayout());
    pipelineConfig.descriptorSetLayouts.push_back(geometrySetLayout);
    mainPipeline->init(device, pipelineConfig);

    // Text pipeline - see main.cpp lines 239-292 for full configuration
    // (Abbreviated for brevity - full implementation needed)
    GraphicsPipelineConfig textPipelineConfig;
    textPipelineConfig.vertexShader = textVertShader.get();
    textPipelineConfig.fragmentShader = textFragShader.get();
    textPipelineConfig.colorFormat = swapchain->getImageFormat();
    textPipelineConfig.depthFormat = depthBuffer->getFormat();
    textPipelineConfig.depthTest = false;
    textPipelineConfig.depthWrite = false;
    textPipelineConfig.cullMode = VK_CULL_MODE_NONE;
    textPipelineConfig.blendEnable = true;

    // TextVertex format
    VkVertexInputBindingDescription textBinding{};
    textBinding.binding = 0;
    textBinding.stride = sizeof(TextVertex);
    textBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    textPipelineConfig.vertexBindings.push_back(textBinding);

    VkVertexInputAttributeDescription textPosAttr{};
    textPosAttr.location = 0;
    textPosAttr.binding = 0;
    textPosAttr.format = VK_FORMAT_R32G32_SFLOAT;
    textPosAttr.offset = offsetof(TextVertex, position);
    textPipelineConfig.vertexAttributes.push_back(textPosAttr);

    VkVertexInputAttributeDescription textTexCoordAttr{};
    textTexCoordAttr.location = 1;
    textTexCoordAttr.binding = 0;
    textTexCoordAttr.format = VK_FORMAT_R32G32_SFLOAT;
    textTexCoordAttr.offset = offsetof(TextVertex, texCoord);
    textPipelineConfig.vertexAttributes.push_back(textTexCoordAttr);

    VkVertexInputAttributeDescription textColorAttr{};
    textColorAttr.location = 2;
    textColorAttr.binding = 0;
    textColorAttr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    textColorAttr.offset = offsetof(TextVertex, color);
    textPipelineConfig.vertexAttributes.push_back(textColorAttr);

    VkVertexInputAttributeDescription textTextureIndexAttr{};
    textTextureIndexAttr.location = 3;
    textTextureIndexAttr.binding = 0;
    textTextureIndexAttr.format = VK_FORMAT_R32_UINT;
    textTextureIndexAttr.offset = offsetof(TextVertex, textureIndex);
    textPipelineConfig.vertexAttributes.push_back(textTextureIndexAttr);

    textPipelineConfig.descriptorSetLayouts.push_back(textureManager.getDescriptorSetLayout());
    textPipeline->init(device, textPipelineConfig);

    // Panel pipeline
    GraphicsPipelineConfig panelPipelineConfig;
    panelPipelineConfig.vertexShader = panelVertShader.get();
    panelPipelineConfig.fragmentShader = panelFragShader.get();
    panelPipelineConfig.colorFormat = swapchain->getImageFormat();
    panelPipelineConfig.depthFormat = depthBuffer->getFormat();
    panelPipelineConfig.depthTest = false;
    panelPipelineConfig.depthWrite = false;
    panelPipelineConfig.cullMode = VK_CULL_MODE_NONE;
    panelPipelineConfig.blendEnable = true;

    VkVertexInputBindingDescription panelBinding{};
    panelBinding.binding = 0;
    panelBinding.stride = sizeof(PanelVertex);
    panelBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    panelPipelineConfig.vertexBindings.push_back(panelBinding);

    VkVertexInputAttributeDescription panelPosAttr{};
    panelPosAttr.location = 0;
    panelPosAttr.binding = 0;
    panelPosAttr.format = VK_FORMAT_R32G32_SFLOAT;
    panelPosAttr.offset = offsetof(PanelVertex, position);
    panelPipelineConfig.vertexAttributes.push_back(panelPosAttr);

    VkVertexInputAttributeDescription panelColorAttr{};
    panelColorAttr.location = 1;
    panelColorAttr.binding = 0;
    panelColorAttr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    panelColorAttr.offset = offsetof(PanelVertex, color);
    panelPipelineConfig.vertexAttributes.push_back(panelColorAttr);

    panelPipeline->init(device, panelPipelineConfig);

    // Outline pipeline
    GraphicsPipelineConfig outlinePipelineConfig;
    outlinePipelineConfig.vertexShader = outlineVertShader.get();
    outlinePipelineConfig.fragmentShader = outlineFragShader.get();
    outlinePipelineConfig.colorFormat = swapchain->getImageFormat();
    outlinePipelineConfig.depthFormat = depthBuffer->getFormat();
    outlinePipelineConfig.depthTest = true;
    outlinePipelineConfig.depthWrite = false;
    outlinePipelineConfig.cullMode = VK_CULL_MODE_NONE;
    outlinePipelineConfig.blendEnable = true;
    outlinePipelineConfig.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    outlinePipelineConfig.lineWidth = 2.0f;

    VkVertexInputBindingDescription outlineBinding{};
    outlineBinding.binding = 0;
    outlineBinding.stride = sizeof(glm::vec3);
    outlineBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    outlinePipelineConfig.vertexBindings.push_back(outlineBinding);

    VkVertexInputAttributeDescription outlinePosAttr{};
    outlinePosAttr.location = 0;
    outlinePosAttr.binding = 0;
    outlinePosAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
    outlinePosAttr.offset = 0;
    outlinePipelineConfig.vertexAttributes.push_back(outlinePosAttr);

    VkPushConstantRange outlinePushConstantRange{};
    outlinePushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    outlinePushConstantRange.offset = 0;
    outlinePushConstantRange.size = 96;
    outlinePipelineConfig.pushConstantRanges.push_back(outlinePushConstantRange);

    outlinePipeline->init(device, outlinePipelineConfig);

    // Blur pipeline
    GraphicsPipelineConfig blurPipelineConfig;
    blurPipelineConfig.vertexShader = blurVertShader.get();
    blurPipelineConfig.fragmentShader = blurFragShader.get();
    blurPipelineConfig.colorFormat = swapchain->getImageFormat();
    blurPipelineConfig.depthFormat = VK_FORMAT_UNDEFINED;
    blurPipelineConfig.depthTest = false;
    blurPipelineConfig.depthWrite = false;
    blurPipelineConfig.cullMode = VK_CULL_MODE_NONE;
    blurPipelineConfig.blendEnable = false;
    blurPipelineConfig.descriptorSetLayouts.push_back(textureManager.getDescriptorSetLayout());

    VkPushConstantRange blurPushConstantRange{};
    blurPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    blurPushConstantRange.offset = 0;
    blurPushConstantRange.size = 24;
    blurPipelineConfig.pushConstantRanges.push_back(blurPushConstantRange);

    blurPipeline->init(device, blurPipelineConfig);
}

void RenderManager::createBuffers() {
    // Quad info buffer
    quadInfoBuffer->init(
        vulkanContext->getAllocator(),
        16384 * sizeof(QuadInfo),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    // Text vertex buffer
    textVertexBuffer->init(vulkanContext->getAllocator(), 100000 * sizeof(TextVertex),
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          VMA_MEMORY_USAGE_CPU_TO_GPU,
                          VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    // Panel vertex buffer
    panelVertexBuffer->init(vulkanContext->getAllocator(), 10000 * sizeof(PanelVertex),
                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                           VMA_MEMORY_USAGE_CPU_TO_GPU,
                           VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    // Outline vertex buffer
    outlineVertexBuffer->init(vulkanContext->getAllocator(), 24 * sizeof(glm::vec3),
                             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    // Chunk buffer manager
    bufferManager->init(vulkanContext->getAllocator(), 10000000, 5000);

    // Create descriptor pool
    VkDescriptorPoolSize geometryPoolSizes[] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4}
    };

    VkDescriptorPoolCreateInfo geometryPoolInfo{};
    geometryPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    geometryPoolInfo.maxSets = 1;
    geometryPoolInfo.poolSizeCount = 1;
    geometryPoolInfo.pPoolSizes = geometryPoolSizes;

    vkCreateDescriptorPool(vulkanContext->getDevice().getLogicalDevice(), &geometryPoolInfo, nullptr, &geometryDescriptorPool);

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo geometryAllocInfo{};
    geometryAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    geometryAllocInfo.descriptorPool = geometryDescriptorPool;
    geometryAllocInfo.descriptorSetCount = 1;
    geometryAllocInfo.pSetLayouts = &geometrySetLayout;

    vkAllocateDescriptorSets(vulkanContext->getDevice().getLogicalDevice(), &geometryAllocInfo, &geometryDescriptorSet);
}

bool RenderManager::beginFrame() {
    return renderer->beginFrame();
}

void RenderManager::endFrame() {
    renderer->endFrame();
}

void RenderManager::waitIdle() {
    vulkanContext->waitIdle();
}

void RenderManager::clearChunkBuffers() {
    waitIdle();
    bufferManager->clear();
    quadInfoNeedsUpdate = true;
}

void RenderManager::onResize(uint32_t width, uint32_t height, TextureManager& textureManager) {
    waitIdle();
    swapchain->recreate(width, height);
    depthBuffer->resize(vulkanContext->getAllocator(), vulkanContext->getDevice().getLogicalDevice(),
                       width, height);

    // Resize offscreen targets
    sceneTarget->resize(width, height);
    blurTarget1->resize(width, height);

    // Re-register external textures after resize
    textureManager.updateExternalTexture(sceneTextureIndex, sceneTarget->getColorImageView());
    textureManager.updateExternalTexture(blurTexture1Index, blurTarget1->getColorImageView());
}

void RenderManager::render(Camera& camera, ChunkManager& chunkManager,
                          GameStateManager& gameStateManager, Settings& settings,
                          TextureManager& textureManager,
                          const std::optional<BlockHitResult>& crosshairTarget) {
    // Update QuadInfo buffer if needed
    if (quadInfoNeedsUpdate) {
        const auto& quadInfos = chunkManager.getQuadInfos();
        if (!quadInfos.empty()) {
            waitIdle();

            void* quadData = quadInfoBuffer->map();
            memcpy(quadData, quadInfos.data(), quadInfos.size() * sizeof(QuadInfo));
            quadInfoBuffer->unmap();

            // Update descriptor sets
            VkDescriptorBufferInfo quadInfoBufferInfo{};
            quadInfoBufferInfo.buffer = quadInfoBuffer->getBuffer();
            quadInfoBufferInfo.offset = 0;
            quadInfoBufferInfo.range = quadInfos.size() * sizeof(QuadInfo);

            VkDescriptorBufferInfo lightingBufferInfo{};
            lightingBufferInfo.buffer = bufferManager->getLightingBuffer();
            lightingBufferInfo.offset = 0;
            lightingBufferInfo.range = VK_WHOLE_SIZE;

            VkDescriptorBufferInfo chunkDataBufferInfo{};
            chunkDataBufferInfo.buffer = bufferManager->getChunkDataBuffer();
            chunkDataBufferInfo.offset = 0;
            chunkDataBufferInfo.range = VK_WHOLE_SIZE;

            VkDescriptorBufferInfo faceDataBufferInfo{};
            faceDataBufferInfo.buffer = bufferManager->getFaceBuffer();
            faceDataBufferInfo.offset = 0;
            faceDataBufferInfo.range = VK_WHOLE_SIZE;

            VkWriteDescriptorSet descriptorWrites[4]{};
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = geometryDescriptorSet;
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &quadInfoBufferInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = geometryDescriptorSet;
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pBufferInfo = &lightingBufferInfo;

            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = geometryDescriptorSet;
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pBufferInfo = &chunkDataBufferInfo;

            descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[3].dstSet = geometryDescriptorSet;
            descriptorWrites[3].dstBinding = 3;
            descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[3].descriptorCount = 1;
            descriptorWrites[3].pBufferInfo = &faceDataBufferInfo;

            vkUpdateDescriptorSets(vulkanContext->getDevice().getLogicalDevice(), 4, descriptorWrites, 0, nullptr);
            quadInfoNeedsUpdate = false;
        }
    }

    auto cmd = renderer->getCurrentCommandBuffer();

    // Determine if blur is needed
    auto currentState = gameStateManager.getState();
    bool needsBlur = (currentState == GameStateManager::State::Paused ||
                     currentState == GameStateManager::State::Options) &&
                     settings.menuBlurAmount > 0;

    // Render scene to offscreen target if blur is needed, otherwise directly to swapchain
    VkImageView renderTarget = needsBlur ? sceneTarget->getColorImageView() :
                                swapchain->getImageViews()[renderer->getCurrentImageIndex()];
    VkImageView depthTarget = needsBlur ? sceneTarget->getDepthImageView() :
                               depthBuffer->getImageView();

    cmd.beginRendering(
        renderTarget,
        swapchain->getExtent(),
        glm::vec4(0.1f, 0.1f, 0.1f, 1.0f),
        depthTarget
    );

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain->getExtent().width);
    viewport.height = static_cast<float>(swapchain->getExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    cmd.setViewport(viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain->getExtent();
    cmd.setScissor(scissor);

    renderScene(camera, chunkManager, gameStateManager, crosshairTarget, textureManager, cmd, viewport);
    renderUI(gameStateManager, settings, camera, needsBlur, currentState, textureManager, cmd);

    cmd.endRendering();

    if (needsBlur) {
        applyBlurPostProcessing(settings, gameStateManager, currentState, textureManager, cmd, viewport, scissor);
    }
}

void RenderManager::renderScene(Camera& camera, ChunkManager& chunkManager,
                                GameStateManager& gameStateManager,
                                const std::optional<BlockHitResult>& crosshairTarget,
                                TextureManager& textureManager, CommandBuffer& cmd, const VkViewport& viewport) {
    cmd.bindPipeline(mainPipeline->getPipeline());

    VkDescriptorSet textureDescSet = textureManager.getDescriptorSet();
    VkDescriptorSet descriptorSets[] = {textureDescSet, geometryDescriptorSet};
    vkCmdBindDescriptorSets(cmd.getBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                           mainPipeline->getLayout(), 0, 2, descriptorSets, 0, nullptr);

    PushConstants pushConstants;

    pushConstants.viewProj = camera.getRotationOnlyViewProjectionMatrix();
    glm::vec3 camPos = camera.getPosition();
    pushConstants.cameraPositionInteger = glm::ivec3(
        static_cast<int32_t>(std::floor(camPos.x)),
        static_cast<int32_t>(std::floor(camPos.y)),
        static_cast<int32_t>(std::floor(camPos.z))
    );
    pushConstants.cameraPositionFraction = glm::vec3(
        camPos.x - std::floor(camPos.x),
        camPos.y - std::floor(camPos.y),
        camPos.z - std::floor(camPos.z)
    );

    cmd.pushConstants(mainPipeline->getLayout(), VK_SHADER_STAGE_VERTEX_BIT,
                     0, sizeof(PushConstants), &pushConstants);

    // Render chunks
    uint32_t drawCount = bufferManager->getDrawCommandCount();
    if (drawCount > 0) {
        static bool loggedOnce = false;
        if (!loggedOnce) {
            spdlog::info("Rendering {} chunks with {} draw commands",
                        bufferManager->getMeshCache().size(), drawCount);
            loggedOnce = true;
        }

        vkCmdDrawIndirect(cmd.getBuffer(), bufferManager->getIndirectBuffer(),
                         0, drawCount, sizeof(VkDrawIndirectCommand));
    }

    // Render block outline
    if (crosshairTarget.has_value() && gameStateManager.isPlaying()) {
        renderBlockOutline(crosshairTarget.value(), cmd, pushConstants);
    }
}

void RenderManager::renderBlockOutline(const BlockHitResult& target, CommandBuffer& cmd,
                                      const PushConstants& pushConstants) {
    const float OUTLINE_OFFSET = 0.002f;
    glm::vec3 blockPos = glm::vec3(target.blockPos);

    const Block* block = BlockRegistry::getBlock(target.state);
    BlockShape shape = block->getOutlineShape(target.state);

    glm::vec3 shapeMin = shape.getMin();
    glm::vec3 shapeMax = shape.getMax();

    glm::vec3 minBound = blockPos + shapeMin - glm::vec3(OUTLINE_OFFSET);
    glm::vec3 maxBound = blockPos + shapeMax + glm::vec3(OUTLINE_OFFSET);

    glm::vec3 outlineVertices[24] = {
        glm::vec3(minBound.x, minBound.y, minBound.z),
        glm::vec3(maxBound.x, minBound.y, minBound.z),

        glm::vec3(maxBound.x, minBound.y, minBound.z),
        glm::vec3(maxBound.x, minBound.y, maxBound.z),

        glm::vec3(maxBound.x, minBound.y, maxBound.z),
        glm::vec3(minBound.x, minBound.y, maxBound.z),

        glm::vec3(minBound.x, minBound.y, maxBound.z),
        glm::vec3(minBound.x, minBound.y, minBound.z),

        glm::vec3(minBound.x, maxBound.y, minBound.z),
        glm::vec3(maxBound.x, maxBound.y, minBound.z),

        glm::vec3(maxBound.x, maxBound.y, minBound.z),
        glm::vec3(maxBound.x, maxBound.y, maxBound.z),

        glm::vec3(maxBound.x, maxBound.y, maxBound.z),
        glm::vec3(minBound.x, maxBound.y, maxBound.z),

        glm::vec3(minBound.x, maxBound.y, maxBound.z),
        glm::vec3(minBound.x, maxBound.y, minBound.z),

        glm::vec3(minBound.x, minBound.y, minBound.z),
        glm::vec3(minBound.x, maxBound.y, minBound.z),

        glm::vec3(maxBound.x, minBound.y, minBound.z),
        glm::vec3(maxBound.x, maxBound.y, minBound.z),

        glm::vec3(maxBound.x, minBound.y, maxBound.z),
        glm::vec3(maxBound.x, maxBound.y, maxBound.z),

        glm::vec3(minBound.x, minBound.y, maxBound.z),
        glm::vec3(minBound.x, maxBound.y, maxBound.z),
    };

    void* outlineData = outlineVertexBuffer->map();
    memcpy(outlineData, outlineVertices, sizeof(outlineVertices));

    cmd.bindPipeline(outlinePipeline->getPipeline());
    cmd.pushConstants(outlinePipeline->getLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);
    cmd.bindVertexBuffer(outlineVertexBuffer->getBuffer());
    cmd.draw(24, 1, 0, 0);
}

void RenderManager::renderUI(GameStateManager& gameStateManager, Settings& settings,
                            Camera& camera, bool needsBlur, GameStateManager::State currentState,
                            TextureManager& textureManager, CommandBuffer& cmd) {
    if (!textureManager.hasFont("default")) {
        return;
    }

    TextRenderer textRenderer;
    textRenderer.init(&textureManager.getFontManager());
    std::vector<TextVertex> allTextVertices;

    if (currentState == GameStateManager::State::MainMenu) {
        auto menuTextVertices = gameStateManager.getMainMenu().generateTextVertices(textRenderer);
        allTextVertices.insert(allTextVertices.end(), menuTextVertices.begin(), menuTextVertices.end());
    } else if (currentState == GameStateManager::State::OptionsFromMain) {
        auto menuTextVertices = gameStateManager.getOptionsMenu().generateTextVertices(textRenderer);
        allTextVertices.insert(allTextVertices.end(), menuTextVertices.begin(), menuTextVertices.end());
    } else if (currentState == GameStateManager::State::Paused && !needsBlur) {
        auto menuTextVertices = gameStateManager.getPauseMenu().generateTextVertices(textRenderer);
        allTextVertices.insert(allTextVertices.end(), menuTextVertices.begin(), menuTextVertices.end());
    } else if (currentState == GameStateManager::State::Options && !needsBlur) {
        auto menuTextVertices = gameStateManager.getOptionsMenu().generateTextVertices(textRenderer);
        allTextVertices.insert(allTextVertices.end(), menuTextVertices.begin(), menuTextVertices.end());
    } else if (!needsBlur) {
        auto titleText = Text::literal("Far Horizon", Style::yellow().withBold(true));

        auto posText = Text::literal("Position: ", Style::gray())
            .append(std::to_string((int)camera.getPosition().x) + ", " +
                   std::to_string((int)camera.getPosition().y) + ", " +
                   std::to_string((int)camera.getPosition().z), Style::white());

        auto titleVertices = textRenderer.generateVertices(titleText, glm::vec2(10, 10), 3.0f,
                                                           getWidth(), getHeight());
        auto posVertices = textRenderer.generateVertices(posText, glm::vec2(10, 80), 2.0f,
                                                        getWidth(), getHeight());

        allTextVertices.insert(allTextVertices.end(), titleVertices.begin(), titleVertices.end());
        allTextVertices.insert(allTextVertices.end(), posVertices.begin(), posVertices.end());
    }

    if (!allTextVertices.empty()) {
        void* data = textVertexBuffer->map();
        memcpy(data, allTextVertices.data(), allTextVertices.size() * sizeof(TextVertex));

        cmd.bindPipeline(textPipeline->getPipeline());
        VkDescriptorSet textDescSet = textureManager.getDescriptorSet();
        cmd.bindDescriptorSets(textPipeline->getLayout(), 0, 1, &textDescSet);
        cmd.bindVertexBuffer(textVertexBuffer->getBuffer());
        cmd.draw(allTextVertices.size(), 1, 0, 0);
    }
}

void RenderManager::applyBlurPostProcessing(Settings& settings, GameStateManager& gameStateManager,
                                           GameStateManager::State currentState, TextureManager& textureManager,
                                           CommandBuffer& cmd, const VkViewport& viewport, const VkRect2D& scissor) {
    VkImageMemoryBarrier sceneBarrier{};
    sceneBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    sceneBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    sceneBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    sceneBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    sceneBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    sceneBarrier.image = sceneTarget->getColorImage();
    sceneBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    sceneBarrier.subresourceRange.baseMipLevel = 0;
    sceneBarrier.subresourceRange.levelCount = 1;
    sceneBarrier.subresourceRange.baseArrayLayer = 0;
    sceneBarrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(cmd.getBuffer(),
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &sceneBarrier);

    cmd.beginRendering(
        blurTarget1->getColorImageView(),
        swapchain->getExtent(),
        glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
        VK_NULL_HANDLE
    );

    cmd.setViewport(viewport);
    cmd.setScissor(scissor);
    cmd.bindPipeline(blurPipeline->getPipeline());
    VkDescriptorSet blurDescSet1 = textureManager.getDescriptorSet();
    cmd.bindDescriptorSets(blurPipeline->getLayout(), 0, 1, &blurDescSet1);

    struct BlurPushConstants {
        uint32_t textureIndex;
        glm::vec2 blurDir;
        float radius;
        float _pad;
    } blurPC;
    blurPC.textureIndex = sceneTextureIndex;
    blurPC.blurDir = glm::vec2(1.0f, 0.0f);
    blurPC.radius = static_cast<float>(settings.menuBlurAmount);
    blurPC._pad = 0.0f;

    cmd.pushConstants(blurPipeline->getLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BlurPushConstants), &blurPC);
    cmd.draw(3, 1, 0, 0);
    cmd.endRendering();

    VkImageMemoryBarrier blur1Barrier{};
    blur1Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    blur1Barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    blur1Barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    blur1Barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    blur1Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    blur1Barrier.image = blurTarget1->getColorImage();
    blur1Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blur1Barrier.subresourceRange.baseMipLevel = 0;
    blur1Barrier.subresourceRange.levelCount = 1;
    blur1Barrier.subresourceRange.baseArrayLayer = 0;
    blur1Barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(cmd.getBuffer(),
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &blur1Barrier);

    cmd.beginRendering(
        swapchain->getImageViews()[renderer->getCurrentImageIndex()],
        swapchain->getExtent(),
        glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
        VK_NULL_HANDLE
    );

    cmd.setViewport(viewport);
    cmd.setScissor(scissor);
    cmd.bindPipeline(blurPipeline->getPipeline());
    VkDescriptorSet blurDescSet2 = textureManager.getDescriptorSet();
    cmd.bindDescriptorSets(blurPipeline->getLayout(), 0, 1, &blurDescSet2);

    blurPC.textureIndex = blurTexture1Index;
    blurPC.blurDir = glm::vec2(0.0f, 1.0f);

    cmd.pushConstants(blurPipeline->getLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BlurPushConstants), &blurPC);
    cmd.draw(3, 1, 0, 0);

    if (textureManager.hasFont("default")) {
        TextRenderer textRenderer;
        textRenderer.init(&textureManager.getFontManager());
        std::vector<TextVertex> menuTextVertices;

        if (currentState == GameStateManager::State::Paused) {
            menuTextVertices = gameStateManager.getPauseMenu().generateTextVertices(textRenderer);
        } else if (currentState == GameStateManager::State::Options) {
            std::vector<PanelVertex> panelVertices = gameStateManager.getOptionsMenu().generatePanelVertices(getWidth(), getHeight());
            if (!panelVertices.empty()) {
                void* panelData = panelVertexBuffer->map();
                memcpy(panelData, panelVertices.data(), panelVertices.size() * sizeof(PanelVertex));
                cmd.bindPipeline(panelPipeline->getPipeline());
                cmd.bindVertexBuffer(panelVertexBuffer->getBuffer());
                cmd.draw(panelVertices.size(), 1, 0, 0);
            }
            menuTextVertices = gameStateManager.getOptionsMenu().generateTextVertices(textRenderer);
        }

        if (!menuTextVertices.empty()) {
            void* data = textVertexBuffer->map();
            memcpy(data, menuTextVertices.data(), menuTextVertices.size() * sizeof(TextVertex));
            cmd.bindPipeline(textPipeline->getPipeline());
            VkDescriptorSet textDescSet = textureManager.getDescriptorSet();
            cmd.bindDescriptorSets(textPipeline->getLayout(), 0, 1, &textDescSet);
            cmd.bindVertexBuffer(textVertexBuffer->getBuffer());
            cmd.draw(menuTextVertices.size(), 1, 0, 0);
        }
    }

    cmd.endRendering();

    sceneBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    sceneBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    sceneBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    sceneBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    blur1Barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    blur1Barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    blur1Barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    blur1Barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkImageMemoryBarrier barriers[] = {sceneBarrier, blur1Barrier};
    vkCmdPipelineBarrier(cmd.getBuffer(),
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 2, barriers);
}

} // namespace FarHorizon