#include <iostream>
#include <chrono>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "core/Window.hpp"
#include "core/InputSystem.hpp"
#include "core/Camera.hpp"
#include "core/Settings.hpp"
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
#include "world/ChunkGpuData.hpp"
#include "world/BlockRegistry.hpp"
#include "text/FontManager.hpp"
#include "text/TextRenderer.hpp"
#include "text/Text.hpp"
#include "ui/PauseMenu.hpp"
#include "ui/MainMenu.hpp"
#include "ui/OptionsMenu.hpp"

using namespace VoxelEngine;

enum class GameState {
    MainMenu,
    Playing,
    Paused,
    Options,
    OptionsFromMain
};

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

        // Load settings from file
        Settings settings;
        settings.load();

        // Initialize block registry before loading models
        BlockRegistry::init();
        spdlog::info("Initialized block registry");

        // Initialize block models first to discover required textures
        ChunkManager chunkManager;
        chunkManager.setRenderDistance(settings.renderDistance);
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

        // Create descriptor set layout for QuadInfo, Lighting, and ChunkData buffers (set 1)
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

        VkDescriptorSetLayout geometrySetLayout;
        vkCreateDescriptorSetLayout(vulkanContext.getDevice().getLogicalDevice(), &geometryLayoutInfo, nullptr, &geometrySetLayout);

        GraphicsPipelineConfig pipelineConfig;
        pipelineConfig.vertexShader = &vertShader;
        pipelineConfig.fragmentShader = &fragShader;
        pipelineConfig.colorFormat = swapchain.getImageFormat();
        pipelineConfig.depthFormat = depthBuffer.getFormat();
        pipelineConfig.depthTest = true;
        pipelineConfig.depthWrite = true;
        pipelineConfig.cullMode = VK_CULL_MODE_BACK_BIT;

        // No vertex bindings needed - FaceData is now an SSBO, not vertex attributes

        // Descriptor set layouts: set 0 = textures, set 1 = geometry (QuadInfo + Lighting + ChunkData + FaceData)
        pipelineConfig.descriptorSetLayouts.push_back(textureManager.getDescriptorSetLayout());
        pipelineConfig.descriptorSetLayouts.push_back(geometrySetLayout);

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

        // Load panel shaders and create panel pipeline
        Shader panelVertShader, panelFragShader;
        panelVertShader.loadFromFile(vulkanContext.getDevice().getLogicalDevice(), "assets/minecraft/shaders/panel.vsh.spv");
        panelFragShader.loadFromFile(vulkanContext.getDevice().getLogicalDevice(), "assets/minecraft/shaders/panel.fsh.spv");

        GraphicsPipelineConfig panelPipelineConfig;
        panelPipelineConfig.vertexShader = &panelVertShader;
        panelPipelineConfig.fragmentShader = &panelFragShader;
        panelPipelineConfig.colorFormat = swapchain.getImageFormat();
        panelPipelineConfig.depthFormat = depthBuffer.getFormat();
        panelPipelineConfig.depthTest = false;  // Panels rendered without depth
        panelPipelineConfig.depthWrite = false;
        panelPipelineConfig.cullMode = VK_CULL_MODE_NONE;
        panelPipelineConfig.blendEnable = true;  // Enable alpha blending for panels

        // PanelVertex format
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

        // No descriptor sets needed for panels (no textures)

        GraphicsPipeline panelPipeline;
        panelPipeline.init(vulkanContext.getDevice().getLogicalDevice(), panelPipelineConfig);

        // Create a buffer for panel vertices
        Buffer panelVertexBuffer;
        panelVertexBuffer.init(vulkanContext.getAllocator(), 10000 * sizeof(PanelVertex),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                              VMA_MEMORY_USAGE_CPU_TO_GPU,
                              VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

        Camera camera;
        float aspectRatio = static_cast<float>(window.getWidth()) / static_cast<float>(window.getHeight());
        camera.init(glm::vec3(0.0f, 20.0f, 0.0f), aspectRatio, settings.fov);
        camera.setKeybinds(settings.keybinds);  // Apply keybinds from settings

        // Initialize chunk buffer manager (now uses compact format: faces instead of vertices/indices)
        ChunkBufferManager bufferManager;
        bufferManager.init(vulkanContext.getAllocator(), 10000000, 5000);  // maxFaces, maxDrawCommands

        // Create global QuadInfo buffer (shared geometry for all chunks)
        // QuadInfo is 120 bytes with std430 layout (verified at compile time)
        Buffer quadInfoBuffer;
        quadInfoBuffer.init(
            vulkanContext.getAllocator(),
            16384 * sizeof(QuadInfo),  // Support up to 16K unique quad geometries
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
        );

        // Create descriptor pool for geometry buffers
        VkDescriptorPoolSize geometryPoolSizes[] = {
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4}  // QuadInfo + Lighting + ChunkData + FaceData
        };

        VkDescriptorPoolCreateInfo geometryPoolInfo{};
        geometryPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        geometryPoolInfo.maxSets = 1;
        geometryPoolInfo.poolSizeCount = 1;
        geometryPoolInfo.pPoolSizes = geometryPoolSizes;

        VkDescriptorPool geometryDescriptorPool;
        vkCreateDescriptorPool(vulkanContext.getDevice().getLogicalDevice(), &geometryPoolInfo, nullptr, &geometryDescriptorPool);

        // Allocate geometry descriptor set
        VkDescriptorSetAllocateInfo geometryAllocInfo{};
        geometryAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        geometryAllocInfo.descriptorPool = geometryDescriptorPool;
        geometryAllocInfo.descriptorSetCount = 1;
        geometryAllocInfo.pSetLayouts = &geometrySetLayout;

        VkDescriptorSet geometryDescriptorSet;
        vkAllocateDescriptorSets(vulkanContext.getDevice().getLogicalDevice(), &geometryAllocInfo, &geometryDescriptorSet);

        spdlog::info("Setup complete, entering render loop...");

        // Initialize menus
        MainMenu mainMenu(window.getWidth(), window.getHeight());
        PauseMenu pauseMenu(window.getWidth(), window.getHeight());
        OptionsMenu optionsMenu(window.getWidth(), window.getHeight(), &camera, &chunkManager, &settings);
        GameState gameState = GameState::MainMenu;

        bool framebufferResized = false;
        window.setResizeCallback([&framebufferResized, &camera, &pauseMenu, &mainMenu, &optionsMenu](uint32_t width, uint32_t height) {
            framebufferResized = true;
            camera.setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
            pauseMenu.onResize(width, height);
            mainMenu.onResize(width, height);
            optionsMenu.onResize(width, height);
        });

        auto lastTime = std::chrono::high_resolution_clock::now();
        std::vector<CompactChunkMesh> pendingMeshes;
        bool quadInfoNeedsUpdate = true;  // Flag to track when QuadInfo buffer needs updating

        while (!window.shouldClose()) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            window.pollEvents();
            InputSystem::processEvents();

            // Handle game state updates
            if (gameState == GameState::MainMenu) {
                // Update main menu
                auto action = mainMenu.update(deltaTime);
                switch (action) {
                    case MainMenu::Action::Singleplayer:
                        gameState = GameState::Playing;
                        spdlog::info("Starting singleplayer game");
                        break;
                    case MainMenu::Action::OpenOptions:
                        gameState = GameState::OptionsFromMain;
                        optionsMenu.reset();
                        spdlog::info("Opening options menu from main menu");
                        break;
                    case MainMenu::Action::Quit:
                        window.close();
                        break;
                    case MainMenu::Action::None:
                        break;
                }
            } else if (gameState == GameState::Playing) {
                // Handle ESC key to toggle pause menu (use isKeyDown for single press)
                if (InputSystem::isKeyDown(KeyCode::Escape)) {
                    gameState = GameState::Paused;
                    pauseMenu.reset();
                }

                // Update camera and world
                camera.update(deltaTime);
                chunkManager.update(camera.getPosition());
            } else if (gameState == GameState::Paused) {
                // Update pause menu
                auto action = pauseMenu.update(deltaTime);
                switch (action) {
                    case PauseMenu::Action::Resume:
                        gameState = GameState::Playing;
                        break;
                    case PauseMenu::Action::OpenOptions:
                        gameState = GameState::Options;
                        optionsMenu.reset();
                        spdlog::info("Opening options menu from pause menu");
                        break;
                    case PauseMenu::Action::Quit:
                        gameState = GameState::MainMenu;
                        mainMenu.reset();

                        // Clear world state
                        chunkManager.clearAllChunks();
                        bufferManager.clear();
                        pendingMeshes.clear();

                        // Reset camera to spawn position (preserve FOV and keybinds from settings)
                        camera.init(glm::vec3(0.0f, 20.0f, 0.0f), aspectRatio, settings.fov);
                        camera.setKeybinds(settings.keybinds);

                        // Reset buffer offsets
                        quadInfoNeedsUpdate = true;

                        spdlog::info("Returning to main menu");
                        break;
                    case PauseMenu::Action::None:
                        break;
                }
            } else if (gameState == GameState::Options) {
                // Update options menu (from pause menu)
                auto action = optionsMenu.update(deltaTime);
                if (action == OptionsMenu::Action::Back) {
                    gameState = GameState::Paused;
                    spdlog::info("Returning to pause menu");
                }
                // Update chunk manager to apply render distance changes immediately (only during gameplay)
                chunkManager.update(camera.getPosition());
            } else if (gameState == GameState::OptionsFromMain) {
                // Update options menu (from main menu)
                auto action = optionsMenu.update(deltaTime);
                if (action == OptionsMenu::Action::Back) {
                    gameState = GameState::MainMenu;
                    spdlog::info("Returning to main menu");
                }
                // Don't update chunk manager - game hasn't started yet!
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
                quadInfoNeedsUpdate = true;  // Mark QuadInfo for update after adding meshes
            }

            // Update global QuadInfo buffer when needed (BEFORE rendering starts)
            if (quadInfoNeedsUpdate) {
                const auto& quadInfos = chunkManager.getQuadInfos();
                if (!quadInfos.empty()) {
                    // Wait for GPU to finish using the descriptor set
                    vulkanContext.waitIdle();

                    void* quadData = quadInfoBuffer.map();
                    memcpy(quadData, quadInfos.data(), quadInfos.size() * sizeof(QuadInfo));
                    quadInfoBuffer.unmap();

                    // Update descriptor sets (now safe since GPU is idle)
                    VkDescriptorBufferInfo quadInfoBufferInfo{};
                    quadInfoBufferInfo.buffer = quadInfoBuffer.getBuffer();
                    quadInfoBufferInfo.offset = 0;
                    quadInfoBufferInfo.range = quadInfos.size() * sizeof(QuadInfo);

                    VkDescriptorBufferInfo lightingBufferInfo{};
                    lightingBufferInfo.buffer = bufferManager.getLightingBuffer();
                    lightingBufferInfo.offset = 0;
                    lightingBufferInfo.range = VK_WHOLE_SIZE;

                    VkDescriptorBufferInfo chunkDataBufferInfo{};
                    chunkDataBufferInfo.buffer = bufferManager.getChunkDataBuffer();
                    chunkDataBufferInfo.offset = 0;
                    chunkDataBufferInfo.range = VK_WHOLE_SIZE;

                    VkDescriptorBufferInfo faceDataBufferInfo{};
                    faceDataBufferInfo.buffer = bufferManager.getFaceBuffer();
                    faceDataBufferInfo.offset = 0;
                    faceDataBufferInfo.range = VK_WHOLE_SIZE;

                    VkWriteDescriptorSet descriptorWrites[4]{};

                    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrites[0].dstSet = geometryDescriptorSet;
                    descriptorWrites[0].dstBinding = 0;
                    descriptorWrites[0].dstArrayElement = 0;
                    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    descriptorWrites[0].descriptorCount = 1;
                    descriptorWrites[0].pBufferInfo = &quadInfoBufferInfo;

                    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrites[1].dstSet = geometryDescriptorSet;
                    descriptorWrites[1].dstBinding = 1;
                    descriptorWrites[1].dstArrayElement = 0;
                    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    descriptorWrites[1].descriptorCount = 1;
                    descriptorWrites[1].pBufferInfo = &lightingBufferInfo;

                    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrites[2].dstSet = geometryDescriptorSet;
                    descriptorWrites[2].dstBinding = 2;
                    descriptorWrites[2].dstArrayElement = 0;
                    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    descriptorWrites[2].descriptorCount = 1;
                    descriptorWrites[2].pBufferInfo = &chunkDataBufferInfo;

                    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrites[3].dstSet = geometryDescriptorSet;
                    descriptorWrites[3].dstBinding = 3;
                    descriptorWrites[3].dstArrayElement = 0;
                    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    descriptorWrites[3].descriptorCount = 1;
                    descriptorWrites[3].pBufferInfo = &faceDataBufferInfo;

                    vkUpdateDescriptorSets(vulkanContext.getDevice().getLogicalDevice(), 4, descriptorWrites, 0, nullptr);

                    quadInfoNeedsUpdate = false;
                    spdlog::debug("Updated QuadInfo buffer with {} unique quad geometries", quadInfos.size());
                }
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

            // Bind descriptor sets (set 0 = textures, set 1 = geometry)
            VkDescriptorSet textureDescSet = textureManager.getDescriptorSet();
            VkDescriptorSet descriptorSets[] = {textureDescSet, geometryDescriptorSet};
            vkCmdBindDescriptorSets(cmd.getBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getLayout(),
                                   0, 2, descriptorSets, 0, nullptr);

            // Prepare push constants with camera-relative positioning
            struct PushConstants {
                glm::mat4 viewProj;
                glm::ivec3 cameraPositionInteger;
                float _pad0;
                glm::vec3 cameraPositionFraction;
                float _pad1;
            } pushConstants;

            // Use rotation-only view-projection (translation handled in shader for camera-relative rendering)
            pushConstants.viewProj = camera.getRotationOnlyViewProjectionMatrix();

            // Split camera position for floating-point precision
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

            cmd.pushConstants(
                pipeline.getLayout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(PushConstants),
                &pushConstants
            );

            // Render chunks with new compact format (instanced, non-indexed)
            uint32_t drawCount = bufferManager.getDrawCommandCount();
            if (drawCount > 0) {
                static bool loggedOnce = false;
                if (!loggedOnce) {
                    spdlog::info("Rendering {} chunks with {} draw commands", bufferManager.getMeshCache().size(), drawCount);
                    loggedOnce = true;
                }

                // FaceData is now in SSBO (binding 3), no vertex buffer needed

                // Use non-indexed indirect drawing (6 vertices per face instance)
                vkCmdDrawIndirect(cmd.getBuffer(), bufferManager.getIndirectBuffer(),
                                 0, drawCount, sizeof(VkDrawIndirectCommand));
            }

            // Render panels for options menu (before text so text appears on top)
            if (gameState == GameState::Options || gameState == GameState::OptionsFromMain) {
                std::vector<PanelVertex> allPanelVertices;
                auto panelVertices = optionsMenu.generatePanelVertices(window.getWidth(), window.getHeight());
                allPanelVertices.insert(allPanelVertices.end(), panelVertices.begin(), panelVertices.end());

                if (!allPanelVertices.empty()) {
                    // Upload panel vertices to buffer
                    void* panelData = panelVertexBuffer.map();
                    memcpy(panelData, allPanelVertices.data(), allPanelVertices.size() * sizeof(PanelVertex));

                    // Bind panel pipeline and render
                    cmd.bindPipeline(panelPipeline.getPipeline());
                    cmd.bindVertexBuffer(panelVertexBuffer.getBuffer());
                    cmd.draw(allPanelVertices.size(), 1, 0, 0);
                }
            }

            // Render text overlay (main menu, HUD, pause menu, or options menu)
            if (fontManager.hasFont("default")) {
                std::vector<TextVertex> allTextVertices;

                if (gameState == GameState::MainMenu) {
                    // Render main menu
                    auto menuTextVertices = mainMenu.generateTextVertices(textRenderer);
                    allTextVertices.insert(allTextVertices.end(), menuTextVertices.begin(), menuTextVertices.end());
                } else if (gameState == GameState::Paused) {
                    // Render pause menu
                    auto menuTextVertices = pauseMenu.generateTextVertices(textRenderer);
                    allTextVertices.insert(allTextVertices.end(), menuTextVertices.begin(), menuTextVertices.end());
                } else if (gameState == GameState::Options || gameState == GameState::OptionsFromMain) {
                    // Render options menu
                    auto menuTextVertices = optionsMenu.generateTextVertices(textRenderer);
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

        // Cleanup resources
        vkDestroyDescriptorPool(vulkanContext.getDevice().getLogicalDevice(), geometryDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(vulkanContext.getDevice().getLogicalDevice(), geometrySetLayout, nullptr);
        quadInfoBuffer.cleanup();
        bufferManager.cleanup();
        panelVertexBuffer.cleanup();
        panelPipeline.cleanup();
        panelFragShader.cleanup();
        panelVertShader.cleanup();
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