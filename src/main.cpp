#include <iostream>
#include <chrono>
#include <array>
#include "core/Window.hpp"
#include "core/InputSystem.hpp"
#include "core/Camera.hpp"
#include "renderer/core/VulkanContext.hpp"
#include "renderer/swapchain/Swapchain.hpp"
#include "renderer/RenderContext.hpp"
#include "renderer/pipeline/Shader.hpp"
#include "renderer/pipeline/GraphicsPipeline.hpp"
#include "renderer/memory/Buffer.hpp"

using namespace VoxelEngine;

// Vertex structure for cube
struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
};

int main() {
    try {
        // Create window
        WindowProperties props;
        props.title = "Vulkan Voxel Engine - Triangle Demo";
        props.width = 1600;
        props.height = 900;
        props.vsync = true;
        props.resizable = true;

        Window window(props);

        // Initialize input system
        InputSystem::init(window.getNativeWindow());

        std::cout << "=== Vulkan Voxel Engine - Camera Demo ===" << std::endl;
        std::cout << "Modern Vulkan 1.4 Renderer:" << std::endl;
        std::cout << "  - Dynamic Rendering (no VkRenderPass)" << std::endl;
        std::cout << "  - Synchronization2" << std::endl;
        std::cout << "  - Descriptor Indexing (bindless-ready)" << std::endl;
        std::cout << "  - VMA Memory Management" << std::endl;
        std::cout << "  - Double-buffered frames" << std::endl;
        std::cout << "\nControls:" << std::endl;
        std::cout << "  WASD - Move camera" << std::endl;
        std::cout << "  Arrow Keys - Rotate camera" << std::endl;
        std::cout << "  Space/Shift - Move up/down" << std::endl;
        std::cout << "  ESC - Exit" << std::endl;
        std::cout << "==========================================" << std::endl;

        // Initialize Vulkan
        VulkanContext vulkanContext;
        vulkanContext.init(window.getNativeWindow(), "Vulkan Voxel Engine");

        // Create swapchain
        Swapchain swapchain;
        swapchain.init(vulkanContext, window.getWidth(), window.getHeight());

        // Create render context
        RenderContext renderer;
        renderer.init(vulkanContext, swapchain);

        // Load shaders
        Shader vertShader, fragShader;
        vertShader.loadFromFile(vulkanContext.getDevice().getLogicalDevice(), "assets/minecraft/shaders/triangle.vsh.spv");
        fragShader.loadFromFile(vulkanContext.getDevice().getLogicalDevice(), "assets/minecraft/shaders/triangle.fsh.spv");

        // Define cube vertices (8 unique vertices with different colors per face)
        std::vector<Vertex> vertices = {
            // Front face (red tint)
            {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.3f, 0.3f}},
            {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.3f, 0.3f}},
            {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.3f, 0.3f}},
            {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.3f, 0.3f}},
            // Back face (green tint)
            {{-0.5f, -0.5f, -0.5f}, {0.3f, 1.0f, 0.3f}},
            {{ 0.5f, -0.5f, -0.5f}, {0.3f, 1.0f, 0.3f}},
            {{ 0.5f,  0.5f, -0.5f}, {0.3f, 1.0f, 0.3f}},
            {{-0.5f,  0.5f, -0.5f}, {0.3f, 1.0f, 0.3f}},
            // Left face (blue tint)
            {{-0.5f, -0.5f, -0.5f}, {0.3f, 0.3f, 1.0f}},
            {{-0.5f, -0.5f,  0.5f}, {0.3f, 0.3f, 1.0f}},
            {{-0.5f,  0.5f,  0.5f}, {0.3f, 0.3f, 1.0f}},
            {{-0.5f,  0.5f, -0.5f}, {0.3f, 0.3f, 1.0f}},
            // Right face (yellow tint)
            {{ 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.3f}},
            {{ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f, 0.3f}},
            {{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 0.3f}},
            {{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.3f}},
            // Top face (magenta tint)
            {{-0.5f,  0.5f, -0.5f}, {1.0f, 0.3f, 1.0f}},
            {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.3f, 1.0f}},
            {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.3f, 1.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.3f, 1.0f}},
            // Bottom face (cyan tint)
            {{-0.5f, -0.5f, -0.5f}, {0.3f, 1.0f, 1.0f}},
            {{-0.5f, -0.5f,  0.5f}, {0.3f, 1.0f, 1.0f}},
            {{ 0.5f, -0.5f,  0.5f}, {0.3f, 1.0f, 1.0f}},
            {{ 0.5f, -0.5f, -0.5f}, {0.3f, 1.0f, 1.0f}},
        };

        // Define cube indices (6 faces * 2 triangles * 3 indices = 36 indices)
        std::vector<uint32_t> indices = {
            // Front
            0, 1, 2,  2, 3, 0,
            // Back
            5, 4, 7,  7, 6, 5,
            // Left
            8, 9, 10,  10, 11, 8,
            // Right
            12, 14, 13,  14, 12, 15,
            // Top
            16, 17, 18,  18, 19, 16,
            // Bottom
            21, 20, 23,  23, 22, 21,
        };

        // Create vertex buffer
        Buffer vertexBuffer;
        vertexBuffer.init(
            vulkanContext.getAllocator(),
            vertices.size() * sizeof(Vertex),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            0
        );

        // Create index buffer
        Buffer indexBuffer;
        indexBuffer.init(
            vulkanContext.getAllocator(),
            indices.size() * sizeof(uint32_t),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            0
        );

        // Create staging buffer for upload
        Buffer stagingBuffer;
        stagingBuffer.init(
            vulkanContext.getAllocator(),
            std::max(vertices.size() * sizeof(Vertex), indices.size() * sizeof(uint32_t)),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_ONLY,
            0
        );

        // Upload vertex data using staging buffer
        void* data = stagingBuffer.map();
        memcpy(data, vertices.data(), vertices.size() * sizeof(Vertex));
        stagingBuffer.unmap();

        // Create temporary command pool and buffer for upload
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

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(uploadCmd, &beginInfo);

        VkBufferCopy copyRegion{};
        copyRegion.size = vertices.size() * sizeof(Vertex);
        vkCmdCopyBuffer(uploadCmd, stagingBuffer.getBuffer(), vertexBuffer.getBuffer(), 1, &copyRegion);

        vkEndCommandBuffer(uploadCmd);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &uploadCmd;
        vkQueueSubmit(vulkanContext.getDevice().getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(vulkanContext.getDevice().getGraphicsQueue());

        // Upload index data
        data = stagingBuffer.map();
        memcpy(data, indices.data(), indices.size() * sizeof(uint32_t));
        stagingBuffer.unmap();

        vkResetCommandBuffer(uploadCmd, 0);
        vkBeginCommandBuffer(uploadCmd, &beginInfo);

        copyRegion.size = indices.size() * sizeof(uint32_t);
        vkCmdCopyBuffer(uploadCmd, stagingBuffer.getBuffer(), indexBuffer.getBuffer(), 1, &copyRegion);

        vkEndCommandBuffer(uploadCmd);
        vkQueueSubmit(vulkanContext.getDevice().getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(vulkanContext.getDevice().getGraphicsQueue());

        // Cleanup upload resources
        vkDestroyCommandPool(vulkanContext.getDevice().getLogicalDevice(), uploadPool, nullptr);
        stagingBuffer.cleanup();

        // Create indirect draw buffer
        VkDrawIndexedIndirectCommand indirectCommand{};
        indirectCommand.indexCount = static_cast<uint32_t>(indices.size());
        indirectCommand.instanceCount = 1;
        indirectCommand.firstIndex = 0;
        indirectCommand.vertexOffset = 0;
        indirectCommand.firstInstance = 0;

        Buffer indirectBuffer;
        indirectBuffer.init(
            vulkanContext.getAllocator(),
            sizeof(VkDrawIndexedIndirectCommand),
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
        );
        void* indirectData = indirectBuffer.map();
        memcpy(indirectData, &indirectCommand, sizeof(VkDrawIndexedIndirectCommand));
        indirectBuffer.unmap();

        std::cout << "[Main] Created cube with " << vertices.size() << " vertices and "
                  << indices.size() << " indices (multi-draw indirect)" << std::endl;

        // Create graphics pipeline with vertex input
        GraphicsPipelineConfig pipelineConfig;
        pipelineConfig.vertexShader = &vertShader;
        pipelineConfig.fragmentShader = &fragShader;
        pipelineConfig.colorFormat = swapchain.getImageFormat();
        pipelineConfig.depthTest = false;
        pipelineConfig.cullMode = VK_CULL_MODE_BACK_BIT; // Enable backface culling for cube

        // Define vertex input bindings (one binding for interleaved position and color)
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        pipelineConfig.vertexBindings.push_back(binding);

        // Define vertex input attributes
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

        GraphicsPipeline pipeline;
        pipeline.init(vulkanContext.getDevice().getLogicalDevice(), pipelineConfig);

        // Create camera
        Camera camera;
        float aspectRatio = static_cast<float>(window.getWidth()) / static_cast<float>(window.getHeight());
        camera.init(glm::vec3(0.0f, 0.0f, 3.0f), aspectRatio, 70.0f);

        std::cout << "\n[Main] Setup complete, entering render loop..." << std::endl;

        // Track window resize and update camera aspect ratio
        bool framebufferResized = false;
        window.setResizeCallback([&framebufferResized, &camera](uint32_t width, uint32_t height) {
            framebufferResized = true;
            camera.setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
        });

        // Delta time tracking
        auto lastTime = std::chrono::high_resolution_clock::now();

        // Main loop
        while (!window.shouldClose()) {
            // Calculate delta time
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            window.pollEvents();
            InputSystem::processEvents();

            // Update camera with delta time
            camera.update(deltaTime);

            // Handle window resize
            if (framebufferResized) {
                int width = window.getWidth();
                int height = window.getHeight();

                // Wait for window to be valid size
                while (width == 0 || height == 0) {
                    window.pollEvents();
                    width = window.getWidth();
                    height = window.getHeight();
                }

                vulkanContext.waitIdle();
                swapchain.recreate(width, height);
                framebufferResized = false;
                std::cout << "[Main] Swapchain recreated" << std::endl;
            }

            // Begin frame
            if (!renderer.beginFrame()) {
                // Swapchain out of date, recreate
                vulkanContext.waitIdle();
                swapchain.recreate(window.getWidth(), window.getHeight());
                continue;
            }

            // Get command buffer for this frame
            auto cmd = renderer.getCurrentCommandBuffer();

            // Begin rendering to swapchain image
            cmd.beginRendering(
                swapchain.getImageViews()[renderer.getCurrentImageIndex()],
                swapchain.getExtent(),
                glm::vec4(0.1f, 0.1f, 0.1f, 1.0f) // Dark gray clear color
            );

            // Set dynamic viewport and scissor
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

            // Bind pipeline
            cmd.bindPipeline(pipeline.getPipeline());

            // Push camera view-projection matrix
            glm::mat4 viewProj = camera.getViewProjectionMatrix();
            cmd.pushConstants(
                pipeline.getLayout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(glm::mat4),
                &viewProj
            );

            // Bind vertex and index buffers
            cmd.bindVertexBuffer(vertexBuffer.getBuffer());
            cmd.bindIndexBuffer(indexBuffer.getBuffer());

            // Draw cube using multi-draw indirect
            cmd.drawIndexedIndirect(indirectBuffer.getBuffer(), 0, 1, sizeof(VkDrawIndexedIndirectCommand));

            // End rendering
            cmd.endRendering();

            // End frame (submits and presents)
            renderer.endFrame();

            // Exit on ESC
            if (InputSystem::isKeyDown(KeyCode::Escape)) {
                std::cout << "[Input] ESC pressed - closing window" << std::endl;
                window.close();
            }
        }

        // Wait for GPU to finish
        vulkanContext.waitIdle();

        // Cleanup
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