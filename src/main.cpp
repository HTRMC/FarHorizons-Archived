#include <iostream>
#include <chrono>
#include "core/Window.hpp"
#include "core/InputSystem.hpp"
#include "core/Camera.hpp"
#include "renderer/core/VulkanContext.hpp"
#include "renderer/swapchain/Swapchain.hpp"
#include "renderer/RenderContext.hpp"
#include "renderer/pipeline/Shader.hpp"
#include "renderer/pipeline/GraphicsPipeline.hpp"
#include "renderer/memory/Buffer.hpp"
#include "renderer/texture/BindlessTextureManager.hpp"
#include "world/ChunkManager.hpp"

using namespace VoxelEngine;

int main() {
    try {
        WindowProperties props;
        props.title = "Vulkan Voxel Engine - Infinite Chunks";
        props.width = 1600;
        props.height = 900;
        props.vsync = true;
        props.resizable = true;

        Window window(props);
        InputSystem::init(window.getNativeWindow());

        std::cout << "=== Vulkan Voxel Engine - Infinite Chunks ===" << std::endl;
        std::cout << "Controls:" << std::endl;
        std::cout << "  WASD - Move camera" << std::endl;
        std::cout << "  Arrow Keys - Rotate camera" << std::endl;
        std::cout << "  Space/Shift - Move up/down" << std::endl;
        std::cout << "  ESC - Exit" << std::endl;
        std::cout << "==========================================" << std::endl;

        VulkanContext vulkanContext;
        vulkanContext.init(window.getNativeWindow(), "Vulkan Voxel Engine");

        Swapchain swapchain;
        swapchain.init(vulkanContext, window.getWidth(), window.getHeight());

        RenderContext renderer;
        renderer.init(vulkanContext, swapchain);

        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

        VkImageCreateInfo depthImageInfo{};
        depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImageInfo.format = depthFormat;
        depthImageInfo.extent = {window.getWidth(), window.getHeight(), 1};
        depthImageInfo.mipLevels = 1;
        depthImageInfo.arrayLayers = 1;
        depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo depthAllocInfo{};
        depthAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkImage depthImage;
        VmaAllocation depthAllocation;
        vmaCreateImage(vulkanContext.getAllocator(), &depthImageInfo, &depthAllocInfo, &depthImage, &depthAllocation, nullptr);

        VkImageViewCreateInfo depthViewInfo{};
        depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthViewInfo.image = depthImage;
        depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthViewInfo.format = depthFormat;
        depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthViewInfo.subresourceRange.baseMipLevel = 0;
        depthViewInfo.subresourceRange.levelCount = 1;
        depthViewInfo.subresourceRange.baseArrayLayer = 0;
        depthViewInfo.subresourceRange.layerCount = 1;

        VkImageView depthImageView;
        vkCreateImageView(vulkanContext.getDevice().getLogicalDevice(), &depthViewInfo, nullptr, &depthImageView);

        Shader vertShader, fragShader;
        vertShader.loadFromFile(vulkanContext.getDevice().getLogicalDevice(), "assets/minecraft/shaders/triangle.vsh.spv");
        fragShader.loadFromFile(vulkanContext.getDevice().getLogicalDevice(), "assets/minecraft/shaders/triangle.fsh.spv");

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = vulkanContext.getDevice().getQueueFamilyIndices().graphicsFamily.value();
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VkCommandPool uploadPool;
        vkCreateCommandPool(vulkanContext.getDevice().getLogicalDevice(), &poolInfo, nullptr, &uploadPool);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = uploadPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer uploadCmd;
        vkAllocateCommandBuffers(vulkanContext.getDevice().getLogicalDevice(), &allocInfo, &uploadCmd);

        BindlessTextureManager textureManager;
        textureManager.init(vulkanContext.getDevice().getLogicalDevice(), vulkanContext.getAllocator(), 1024);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(uploadCmd, &beginInfo);
        uint32_t stoneTextureIndex = textureManager.loadTexture("assets/minecraft/textures/block/stone.png", uploadCmd, true);
        vkEndCommandBuffer(uploadCmd);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &uploadCmd;
        vkQueueSubmit(vulkanContext.getDevice().getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(vulkanContext.getDevice().getGraphicsQueue());

        vkDestroyCommandPool(vulkanContext.getDevice().getLogicalDevice(), uploadPool, nullptr);

        GraphicsPipelineConfig pipelineConfig;
        pipelineConfig.vertexShader = &vertShader;
        pipelineConfig.fragmentShader = &fragShader;
        pipelineConfig.colorFormat = swapchain.getImageFormat();
        pipelineConfig.depthFormat = depthFormat;
        pipelineConfig.depthTest = true;
        pipelineConfig.depthWrite = true;
        pipelineConfig.cullMode = VK_CULL_MODE_BACK_BIT;

        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        pipelineConfig.vertexBindings.push_back(binding);

        VkVertexInputAttributeDescription positionAttr{};
        positionAttr.location = 0;
        positionAttr.binding = 0;
        positionAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        positionAttr.offset = offsetof(Vertex, position);
        pipelineConfig.vertexAttributes.push_back(positionAttr);

        VkVertexInputAttributeDescription colorAttr{};
        colorAttr.location = 1;
        colorAttr.binding = 0;
        colorAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        colorAttr.offset = offsetof(Vertex, color);
        pipelineConfig.vertexAttributes.push_back(colorAttr);

        VkVertexInputAttributeDescription texCoordAttr{};
        texCoordAttr.location = 2;
        texCoordAttr.binding = 0;
        texCoordAttr.format = VK_FORMAT_R32G32_SFLOAT;
        texCoordAttr.offset = offsetof(Vertex, texCoord);
        pipelineConfig.vertexAttributes.push_back(texCoordAttr);

        VkVertexInputAttributeDescription textureIndexAttr{};
        textureIndexAttr.location = 3;
        textureIndexAttr.binding = 0;
        textureIndexAttr.format = VK_FORMAT_R32_UINT;
        textureIndexAttr.offset = offsetof(Vertex, textureIndex);
        pipelineConfig.vertexAttributes.push_back(textureIndexAttr);

        pipelineConfig.descriptorSetLayouts.push_back(textureManager.getDescriptorSetLayout());

        GraphicsPipeline pipeline;
        pipeline.init(vulkanContext.getDevice().getLogicalDevice(), pipelineConfig);

        Camera camera;
        float aspectRatio = static_cast<float>(window.getWidth()) / static_cast<float>(window.getHeight());
        camera.init(glm::vec3(0.0f, 20.0f, 0.0f), aspectRatio, 70.0f);

        ChunkManager chunkManager;
        chunkManager.setRenderDistance(8);

        Buffer vertexBuffer;
        Buffer indexBuffer;
        Buffer indirectBuffer;

        size_t maxVertices = 1000000;
        size_t maxIndices = 2000000;
        size_t maxDrawCommands = 1000;

        vertexBuffer.init(
            vulkanContext.getAllocator(),
            maxVertices * sizeof(Vertex),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
        );

        indexBuffer.init(
            vulkanContext.getAllocator(),
            maxIndices * sizeof(uint32_t),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
        );

        indirectBuffer.init(
            vulkanContext.getAllocator(),
            maxDrawCommands * sizeof(VkDrawIndexedIndirectCommand),
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
        );

        std::cout << "\n[Main] Setup complete, entering render loop..." << std::endl;

        bool framebufferResized = false;
        window.setResizeCallback([&framebufferResized, &camera](uint32_t width, uint32_t height) {
            framebufferResized = true;
            camera.setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
        });

        auto lastTime = std::chrono::high_resolution_clock::now();
        uint32_t drawCommandCount = 0;

        while (!window.shouldClose()) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            window.pollEvents();
            InputSystem::processEvents();
            camera.update(deltaTime);

            chunkManager.update(camera.getPosition());

            if (chunkManager.hasChunksChanged()) {
                std::vector<Vertex> allVertices;
                std::vector<uint32_t> allIndices;
                std::vector<VkDrawIndexedIndirectCommand> drawCommands;

                for (const auto& [pos, chunk] : chunkManager.getChunks()) {
                    if (chunk->isEmpty()) continue;

                    ChunkMesh mesh = chunkManager.generateChunkMesh(chunk.get(), stoneTextureIndex);
                    if (mesh.indices.empty()) continue;

                    VkDrawIndexedIndirectCommand cmd{};
                    cmd.indexCount = static_cast<uint32_t>(mesh.indices.size());
                    cmd.instanceCount = 1;
                    cmd.firstIndex = static_cast<uint32_t>(allIndices.size());
                    cmd.vertexOffset = static_cast<int32_t>(allVertices.size());
                    cmd.firstInstance = 0;
                    drawCommands.push_back(cmd);

                    allVertices.insert(allVertices.end(), mesh.vertices.begin(), mesh.vertices.end());
                    allIndices.insert(allIndices.end(), mesh.indices.begin(), mesh.indices.end());
                }

                if (!allVertices.empty()) {
                    void* vertexData = vertexBuffer.map();
                    memcpy(vertexData, allVertices.data(), allVertices.size() * sizeof(Vertex));
                    vertexBuffer.unmap();

                    void* indexData = indexBuffer.map();
                    memcpy(indexData, allIndices.data(), allIndices.size() * sizeof(uint32_t));
                    indexBuffer.unmap();

                    void* indirectData = indirectBuffer.map();
                    memcpy(indirectData, drawCommands.data(), drawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
                    indirectBuffer.unmap();

                    drawCommandCount = static_cast<uint32_t>(drawCommands.size());

                    std::cout << "[ChunkManager] Updated buffers: " << chunkManager.getChunks().size()
                              << " chunks, " << drawCommandCount << " draw commands" << std::endl;
                }

                chunkManager.clearChangedFlag();
            }

            if (framebufferResized) {
                int width = window.getWidth();
                int height = window.getHeight();

                while (width == 0 || height == 0) {
                    window.pollEvents();
                    width = window.getWidth();
                    height = window.getHeight();
                }

                vulkanContext.waitIdle();
                swapchain.recreate(width, height);

                vkDestroyImageView(vulkanContext.getDevice().getLogicalDevice(), depthImageView, nullptr);
                vmaDestroyImage(vulkanContext.getAllocator(), depthImage, depthAllocation);

                depthImageInfo.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
                vmaCreateImage(vulkanContext.getAllocator(), &depthImageInfo, &depthAllocInfo, &depthImage, &depthAllocation, nullptr);

                depthViewInfo.image = depthImage;
                vkCreateImageView(vulkanContext.getDevice().getLogicalDevice(), &depthViewInfo, nullptr, &depthImageView);

                framebufferResized = false;
            }

            if (!renderer.beginFrame()) {
                vulkanContext.waitIdle();
                swapchain.recreate(window.getWidth(), window.getHeight());
                continue;
            }

            auto cmd = renderer.getCurrentCommandBuffer();

            cmd.beginRendering(
                swapchain.getImageViews()[renderer.getCurrentImageIndex()],
                swapchain.getExtent(),
                glm::vec4(0.1f, 0.1f, 0.1f, 1.0f),
                depthImageView
            );

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(swapchain.getExtent().width);
            viewport.height = static_cast<float>(swapchain.getExtent().height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            cmd.setViewport(viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = swapchain.getExtent();
            cmd.setScissor(scissor);

            cmd.bindPipeline(pipeline.getPipeline());

            VkDescriptorSet textureDescSet = textureManager.getDescriptorSet();
            cmd.bindDescriptorSets(pipeline.getLayout(), 0, 1, &textureDescSet);

            glm::mat4 viewProj = camera.getViewProjectionMatrix();
            cmd.pushConstants(
                pipeline.getLayout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(glm::mat4),
                &viewProj
            );

            if (drawCommandCount > 0) {
                cmd.bindVertexBuffer(vertexBuffer.getBuffer());
                cmd.bindIndexBuffer(indexBuffer.getBuffer());
                cmd.drawIndexedIndirect(indirectBuffer.getBuffer(), 0, drawCommandCount, sizeof(VkDrawIndexedIndirectCommand));
            }

            cmd.endRendering();
            renderer.endFrame();

            if (InputSystem::isKeyDown(KeyCode::Escape)) {
                window.close();
            }
        }

        vulkanContext.waitIdle();

        textureManager.shutdown();
        vkDestroyImageView(vulkanContext.getDevice().getLogicalDevice(), depthImageView, nullptr);
        vmaDestroyImage(vulkanContext.getAllocator(), depthImage, depthAllocation);
        indirectBuffer.cleanup();
        indexBuffer.cleanup();
        vertexBuffer.cleanup();
        pipeline.cleanup();
        fragShader.cleanup();
        vertShader.cleanup();
        renderer.shutdown();
        swapchain.shutdown();
        vulkanContext.shutdown();
        InputSystem::shutdown();

        std::cout << "[Main] Application shutting down..." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}