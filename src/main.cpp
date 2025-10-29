#include <iostream>
#include <chrono>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "core/Window.hpp"
#include "core/InputSystem.hpp"
#include "core/Camera.hpp"
#include "renderer/core/VulkanContext.hpp"
#include "renderer/swapchain/Swapchain.hpp"
#include "renderer/RenderContext.hpp"
#include "renderer/pipeline/Shader.hpp"
#include "renderer/pipeline/GraphicsPipeline.hpp"
#include "renderer/memory/Buffer.hpp"
#include "renderer/memory/ChunkBufferManager.hpp"
#include "renderer/texture/BindlessTextureManager.hpp"
#include "renderer/DepthBuffer.hpp"
#include "world/ChunkManager.hpp"
#include "world/BlockRegistry.hpp"
#include "text/FontManager.hpp"
#include "text/TextRenderer.hpp"
#include "text/Text.hpp"
#include "ui/PauseMenu.hpp"

using namespace VoxelEngine;

int main() {
    try {
        spdlog::init_thread_pool(8192, 1);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto logger = std::make_shared<spdlog::async_logger>("main", console_sink, spdlog::thread_pool(), spdlog::async_overflow_policy::block);
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::debug);
        spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

        WindowProperties props;
        props.title = "Vulkan Voxel Engine - Infinite Chunks";
        props.width = 1600;
        props.height = 900;
        props.vsync = true;
        props.resizable = true;

        Window window(props);
        InputSystem::init(window.getNativeWindow());

        spdlog::info("=== Vulkan Voxel Engine - Infinite Chunks ===");
        spdlog::info("Controls:");
        spdlog::info("  WASD - Move camera");
        spdlog::info("  Arrow Keys - Rotate camera");
        spdlog::info("  Space/Shift - Move up/down");
        spdlog::info("  ESC - Pause menu");
        spdlog::info("==========================================");

        VulkanContext vulkanContext;
        vulkanContext.init(window.getNativeWindow(), "Vulkan Voxel Engine");

        Swapchain swapchain;
        swapchain.init(vulkanContext, window.getWidth(), window.getHeight());

        RenderContext renderer;
        renderer.init(vulkanContext, swapchain);

        DepthBuffer depthBuffer;
        depthBuffer.init(vulkanContext.getAllocator(), vulkanContext.getDevice().getLogicalDevice(),
                        window.getWidth(), window.getHeight());

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

        // Initialize block registry before loading models
        BlockRegistry::init();
        spdlog::info("Initialized block registry");

        // Initialize block models first to discover required textures
        ChunkManager chunkManager;
        chunkManager.setRenderDistance(8);
        chunkManager.initializeBlockModels();

        // Preload all blockstate models into cache for fast lookup
        chunkManager.preloadBlockStateModels();

        // Get all textures required by the models
        auto requiredTextures = chunkManager.getRequiredTextures();
        spdlog::info("Found {} unique textures required by block models", requiredTextures.size());

        BindlessTextureManager textureManager;
        textureManager.init(vulkanContext.getDevice().getLogicalDevice(), vulkanContext.getAllocator(), 1024);

        // Load all required textures
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(uploadCmd, &beginInfo);

        for (const auto& textureName : requiredTextures) {
            std::string texturePath = "assets/minecraft/textures/block/" + textureName + ".png";
            spdlog::info("Loading texture: {} -> {}", textureName, texturePath);

            uint32_t textureIndex = textureManager.loadTexture(texturePath, uploadCmd, false);
            chunkManager.registerTexture(textureName, textureIndex);
        }

        // Cache texture indices in block models for fast lookup during meshing
        chunkManager.cacheTextureIndices();

        // Pre-compute BlockShapes for all BlockStates (eliminates first-access stutter)
        chunkManager.precacheBlockShapes();

        // Initialize font manager and load font
        // NOTE: You need to provide a font texture at assets/minecraft/textures/font/ascii.png
        // You can copy it from the Minecraft decompiled folder or create your own
        FontManager fontManager;
        fontManager.init(&textureManager);

        // Try to load the font (won't crash if file doesn't exist, just won't render text)
        fontManager.loadGridFont("default", "assets/minecraft/textures/font/ascii.png",
                                 uploadCmd, 128, 128, 16, 16, 0);

        // Initialize text renderer
        TextRenderer textRenderer;
        textRenderer.init(&fontManager);

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
        pipelineConfig.depthFormat = depthBuffer.getFormat();
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

        // Load text shaders and create text pipeline
        Shader textVertShader, textFragShader;
        textVertShader.loadFromFile(vulkanContext.getDevice().getLogicalDevice(), "assets/minecraft/shaders/text.vsh.spv");
        textFragShader.loadFromFile(vulkanContext.getDevice().getLogicalDevice(), "assets/minecraft/shaders/text.fsh.spv");

        GraphicsPipelineConfig textPipelineConfig;
        textPipelineConfig.vertexShader = &textVertShader;
        textPipelineConfig.fragmentShader = &textFragShader;
        textPipelineConfig.colorFormat = swapchain.getImageFormat();
        textPipelineConfig.depthFormat = depthBuffer.getFormat();  // Must match even if not using depth
        textPipelineConfig.depthTest = false;  // Text rendered without depth
        textPipelineConfig.depthWrite = false;
        textPipelineConfig.cullMode = VK_CULL_MODE_NONE;
        textPipelineConfig.blendEnable = true;  // Enable alpha blending for text

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

        GraphicsPipeline textPipeline;
        textPipeline.init(vulkanContext.getDevice().getLogicalDevice(), textPipelineConfig);

        // Create a buffer for text vertices
        Buffer textVertexBuffer;
        textVertexBuffer.init(vulkanContext.getAllocator(), 100000 * sizeof(TextVertex),
                             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

        Camera camera;
        float aspectRatio = static_cast<float>(window.getWidth()) / static_cast<float>(window.getHeight());
        camera.init(glm::vec3(0.0f, 20.0f, 0.0f), aspectRatio, 70.0f);

        ChunkBufferManager bufferManager;
        bufferManager.init(vulkanContext.getAllocator(), 5000000, 10000000, 5000);

        spdlog::info("Setup complete, entering render loop...");

        // Initialize pause menu
        PauseMenu pauseMenu(window.getWidth(), window.getHeight());
        bool isPaused = false;

        bool framebufferResized = false;
        window.setResizeCallback([&framebufferResized, &camera, &pauseMenu](uint32_t width, uint32_t height) {
            framebufferResized = true;
            camera.setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
            pauseMenu.onResize(width, height);
        });

        auto lastTime = std::chrono::high_resolution_clock::now();
        std::vector<ChunkMesh> pendingMeshes;

        while (!window.shouldClose()) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            window.pollEvents();
            InputSystem::processEvents();

            // Handle ESC key to toggle pause menu
            if (InputSystem::isKeyPressed(KeyCode::Escape)) {
                isPaused = !isPaused;
                if (isPaused) {
                    pauseMenu.reset();
                }
            }

            // Update pause menu when paused
            if (isPaused) {
                auto action = pauseMenu.update(deltaTime);
                switch (action) {
                    case PauseMenu::Action::Resume:
                        isPaused = false;
                        break;
                    case PauseMenu::Action::OpenOptions:
                        // TODO: Open options menu (not implemented yet)
                        spdlog::info("Options menu not yet implemented");
                        break;
                    case PauseMenu::Action::Quit:
                        window.close();
                        break;
                    case PauseMenu::Action::None:
                        break;
                }
            }

            // Only update world when not paused
            if (!isPaused) {
                camera.update(deltaTime);
                chunkManager.update(camera.getPosition());
            }

            // Collect new ready meshes
            if (chunkManager.hasReadyMeshes()) {
                auto readyMeshes = chunkManager.getReadyMeshes();
                pendingMeshes.insert(pendingMeshes.end(),
                                    std::make_move_iterator(readyMeshes.begin()),
                                    std::make_move_iterator(readyMeshes.end()));
            }

            // Remove unloaded chunks
            bufferManager.removeUnloadedChunks(chunkManager);

            // Check for compaction
            bufferManager.compactIfNeeded(bufferManager.getMeshCache());

            // Add pending meshes incrementally
            if (!pendingMeshes.empty()) {
                bufferManager.addMeshes(pendingMeshes, 20);
                // Remove processed meshes (addMeshes processes from the front)
                size_t processCount = std::min(pendingMeshes.size(), size_t(20));
                pendingMeshes.erase(pendingMeshes.begin(), pendingMeshes.begin() + processCount);
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
                depthBuffer.resize(vulkanContext.getAllocator(), vulkanContext.getDevice().getLogicalDevice(),
                                  width, height);

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
                depthBuffer.getImageView()
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

            uint32_t drawCount = bufferManager.getDrawCommandCount();
            if (drawCount > 0) {
                cmd.bindVertexBuffer(bufferManager.getVertexBuffer());
                cmd.bindIndexBuffer(bufferManager.getIndexBuffer());
                cmd.drawIndexedIndirect(bufferManager.getIndirectBuffer(), 0, drawCount, sizeof(VkDrawIndexedIndirectCommand));
            }

            // Render text overlay (HUD or pause menu)
            if (fontManager.hasFont("default")) {
                std::vector<TextVertex> allTextVertices;

                if (isPaused) {
                    // Render pause menu
                    auto menuTextVertices = pauseMenu.generateTextVertices(textRenderer);
                    allTextVertices.insert(allTextVertices.end(), menuTextVertices.begin(), menuTextVertices.end());
                } else {
                    // Calculate FPS
                    static float fpsTimer = 0.0f;
                    static int frameCount = 0;
                    static int fps = 0;
                    fpsTimer += deltaTime;
                    frameCount++;
                    if (fpsTimer >= 1.0f) {
                        fps = frameCount;
                        frameCount = 0;
                        fpsTimer = 0.0f;
                    }

                    // Create example text with styling
                    auto titleText = Text::literal("Vulkan Voxel Engine", Style::yellow().withBold(true));
                    auto fpsText = Text::literal("FPS: ", Style::gray())
                        .append(std::to_string(fps), fps >= 60 ? Style::green() : Style::red());

                    auto posText = Text::literal("Position: ", Style::gray())
                        .append(std::to_string((int)camera.getPosition().x) + ", " +
                               std::to_string((int)camera.getPosition().y) + ", " +
                               std::to_string((int)camera.getPosition().z), Style::white());

                    // Example of legacy formatting codes
                    auto legacyText = Text::parseLegacy("Styled Text: §aGreen §cRed §eYellow §lBold §rReset");

                    // Generate vertices for all text
                    auto titleVertices = textRenderer.generateVertices(titleText, glm::vec2(10, 10), 3.0f,
                                                                       window.getWidth(), window.getHeight());
                    auto fpsVertices = textRenderer.generateVertices(fpsText, glm::vec2(10, 50), 2.0f,
                                                                    window.getWidth(), window.getHeight());
                    auto posVertices = textRenderer.generateVertices(posText, glm::vec2(10, 80), 2.0f,
                                                                    window.getWidth(), window.getHeight());
                    auto legacyVertices = textRenderer.generateVertices(legacyText, glm::vec2(10, 110), 2.0f,
                                                                       window.getWidth(), window.getHeight());

                    // Combine all vertices
                    allTextVertices.insert(allTextVertices.end(), titleVertices.begin(), titleVertices.end());
                    allTextVertices.insert(allTextVertices.end(), fpsVertices.begin(), fpsVertices.end());
                    allTextVertices.insert(allTextVertices.end(), posVertices.begin(), posVertices.end());
                    allTextVertices.insert(allTextVertices.end(), legacyVertices.begin(), legacyVertices.end());
                }

                if (!allTextVertices.empty()) {
                    // Upload vertices to buffer
                    void* data = textVertexBuffer.map();
                    memcpy(data, allTextVertices.data(), allTextVertices.size() * sizeof(TextVertex));

                    // Bind text pipeline and render
                    cmd.bindPipeline(textPipeline.getPipeline());
                    cmd.bindDescriptorSets(textPipeline.getLayout(), 0, 1, &textureDescSet);
                    cmd.bindVertexBuffer(textVertexBuffer.getBuffer());
                    cmd.draw(allTextVertices.size(), 1, 0, 0);
                }
            }

            cmd.endRendering();
            renderer.endFrame();
        }

        vulkanContext.waitIdle();

        bufferManager.cleanup();
        textVertexBuffer.cleanup();
        textPipeline.cleanup();
        textFragShader.cleanup();
        textVertShader.cleanup();
        textureManager.shutdown();
        depthBuffer.cleanup(vulkanContext.getDevice().getLogicalDevice(), vulkanContext.getAllocator());
        pipeline.cleanup();
        fragShader.cleanup();
        vertShader.cleanup();
        renderer.shutdown();
        swapchain.shutdown();
        vulkanContext.shutdown();
        InputSystem::shutdown();

        spdlog::info("Application shutting down...");
        spdlog::shutdown();

    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        spdlog::shutdown();
        return 1;
    }

    return 0;
}