#include <iostream>
#include <chrono>
#include <array>
#include <cmath>
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

using namespace VoxelEngine;

// Vertex structure for voxel mesh
struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 texCoord;
    uint32_t textureIndex;
};

// Chunk data structure
constexpr uint32_t CHUNK_SIZE = 16;
constexpr uint32_t CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
constexpr uint32_t CHUNK_BIT_ARRAY_SIZE = CHUNK_VOLUME / 8; // 4096 bits = 512 bytes

// Helper functions for chunk data
inline uint32_t getVoxelIndex(uint32_t x, uint32_t y, uint32_t z) {
    return x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
}

inline bool getVoxel(const uint8_t* chunkData, uint32_t x, uint32_t y, uint32_t z) {
    if (x >= CHUNK_SIZE || y >= CHUNK_SIZE || z >= CHUNK_SIZE) return false;
    uint32_t index = getVoxelIndex(x, y, z);
    return (chunkData[index / 8] & (1 << (index % 8))) != 0;
}

inline void setVoxel(uint8_t* chunkData, uint32_t x, uint32_t y, uint32_t z, bool value) {
    if (x >= CHUNK_SIZE || y >= CHUNK_SIZE || z >= CHUNK_SIZE) return;
    uint32_t index = getVoxelIndex(x, y, z);
    if (value) {
        chunkData[index / 8] |= (1 << (index % 8));
    } else {
        chunkData[index / 8] &= ~(1 << (index % 8));
    }
}

// Mesh generation for voxel chunk
struct ChunkMesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

// Chunk position in world space
struct ChunkPosition {
    int32_t x, y, z;
};

ChunkMesh generateChunkMesh(const uint8_t* chunkData, uint32_t textureIndex = 0, ChunkPosition chunkPos = {0, 0, 0}) {
    ChunkMesh mesh;

    // Face vertices (relative to block position)
    const glm::vec3 faceVertices[6][4] = {
        // Front (+Z)
        {glm::vec3(0, 0, 1), glm::vec3(1, 0, 1), glm::vec3(1, 1, 1), glm::vec3(0, 1, 1)},
        // Back (-Z)
        {glm::vec3(1, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec3(1, 1, 0)},
        // Left (-X)
        {glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 1), glm::vec3(0, 1, 0)},
        // Right (+X)
        {glm::vec3(1, 0, 1), glm::vec3(1, 0, 0), glm::vec3(1, 1, 0), glm::vec3(1, 1, 1)},
        // Top (+Y)
        {glm::vec3(0, 1, 0), glm::vec3(0, 1, 1), glm::vec3(1, 1, 1), glm::vec3(1, 1, 0)},
        // Bottom (-Y)
        {glm::vec3(0, 0, 1), glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(1, 0, 1)},
    };

    // UV coordinates for each face (0,0 to 1,1)
    const glm::vec2 faceUVs[4] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };

    // Face colors (keep for tinting/debugging)
    const glm::vec3 faceColors[6] = {
        glm::vec3(1.0f, 0.3f, 0.3f), // Front - Red
        glm::vec3(0.3f, 1.0f, 0.3f), // Back - Green
        glm::vec3(0.3f, 0.3f, 1.0f), // Left - Blue
        glm::vec3(1.0f, 1.0f, 0.3f), // Right - Yellow
        glm::vec3(1.0f, 0.3f, 1.0f), // Top - Magenta
        glm::vec3(0.3f, 1.0f, 1.0f), // Bottom - Cyan
    };

    // Neighbor offsets for each face
    const int32_t neighbors[6][3] = {
        {0, 0, 1},   // Front
        {0, 0, -1},  // Back
        {-1, 0, 0},  // Left
        {1, 0, 0},   // Right
        {0, 1, 0},   // Top
        {0, -1, 0},  // Bottom
    };

    // Iterate through all voxels
    for (uint32_t z = 0; z < CHUNK_SIZE; z++) {
        for (uint32_t y = 0; y < CHUNK_SIZE; y++) {
            for (uint32_t x = 0; x < CHUNK_SIZE; x++) {
                if (!getVoxel(chunkData, x, y, z)) continue; // Skip air

                glm::vec3 blockPos(x, y, z);

                // Check each face
                for (int face = 0; face < 6; face++) {
                    int32_t nx = x + neighbors[face][0];
                    int32_t ny = y + neighbors[face][1];
                    int32_t nz = z + neighbors[face][2];

                    // Only render face if neighbor is air or out of bounds
                    bool shouldRender = false;
                    if (nx < 0 || nx >= CHUNK_SIZE || ny < 0 || ny >= CHUNK_SIZE ||
                        nz < 0 || nz >= CHUNK_SIZE) {
                        shouldRender = true; // Out of bounds
                    } else if (!getVoxel(chunkData, nx, ny, nz)) {
                        shouldRender = true; // Neighbor is air
                    }

                    if (shouldRender) {
                        uint32_t startVertex = mesh.vertices.size();

                        // Add 4 vertices for this face (with chunk offset)
                        glm::vec3 chunkOffset(chunkPos.x * CHUNK_SIZE, chunkPos.y * CHUNK_SIZE, chunkPos.z * CHUNK_SIZE);
                        for (int v = 0; v < 4; v++) {
                            Vertex vertex;
                            vertex.position = chunkOffset + blockPos + faceVertices[face][v];
                            vertex.color = faceColors[face];
                            vertex.texCoord = faceUVs[v];
                            vertex.textureIndex = textureIndex;
                            mesh.vertices.push_back(vertex);
                        }

                        // Add 2 triangles (6 indices)
                        mesh.indices.push_back(startVertex + 0);
                        mesh.indices.push_back(startVertex + 1);
                        mesh.indices.push_back(startVertex + 2);

                        mesh.indices.push_back(startVertex + 2);
                        mesh.indices.push_back(startVertex + 3);
                        mesh.indices.push_back(startVertex + 0);
                    }
                }
            }
        }
    }

    return mesh;
}

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

        // Create depth buffer
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

        std::cout << "[Main] Created depth buffer (" << window.getWidth() << "x" << window.getHeight() << ")" << std::endl;

        // Load shaders
        Shader vertShader, fragShader;
        vertShader.loadFromFile(vulkanContext.getDevice().getLogicalDevice(), "assets/minecraft/shaders/triangle.vsh.spv");
        fragShader.loadFromFile(vulkanContext.getDevice().getLogicalDevice(), "assets/minecraft/shaders/triangle.fsh.spv");

        // Generate multiple chunks in a grid
        constexpr int32_t CHUNK_GRID_SIZE = 3; // 3x3x3 grid = 27 chunks
        std::vector<Vertex> allVertices;
        std::vector<uint32_t> allIndices;
        std::vector<VkDrawIndexedIndirectCommand> drawCommands;

        std::cout << "[Main] Generating " << (CHUNK_GRID_SIZE * CHUNK_GRID_SIZE * CHUNK_GRID_SIZE) << " chunks..." << std::endl;

        uint32_t totalChunks = 0;
        for (int32_t cz = 0; cz < CHUNK_GRID_SIZE; cz++) {
            for (int32_t cy = 0; cy < CHUNK_GRID_SIZE; cy++) {
                for (int32_t cx = 0; cx < CHUNK_GRID_SIZE; cx++) {
                    // Create chunk data with interesting pattern
                    uint8_t chunkData[CHUNK_BIT_ARRAY_SIZE] = {0};

                    // Different pattern for each chunk layer
                    if (cy == 0) {
                        // Bottom layer - solid ground
                        for (uint32_t y = 0; y < 2; y++) {
                            for (uint32_t z = 0; z < CHUNK_SIZE; z++) {
                                for (uint32_t x = 0; x < CHUNK_SIZE; x++) {
                                    setVoxel(chunkData, x, y, z, true);
                                }
                            }
                        }
                    } else {
                        // Upper layers - hollow sphere pattern
                        float centerX = CHUNK_SIZE / 2.0f;
                        float centerY = CHUNK_SIZE / 2.0f;
                        float centerZ = CHUNK_SIZE / 2.0f;
                        float outerRadius = 7.0f;
                        float innerRadius = 5.0f;

                        for (uint32_t z = 0; z < CHUNK_SIZE; z++) {
                            for (uint32_t y = 0; y < CHUNK_SIZE; y++) {
                                for (uint32_t x = 0; x < CHUNK_SIZE; x++) {
                                    float dx = x - centerX;
                                    float dy = y - centerY;
                                    float dz = z - centerZ;
                                    float distance = std::sqrt(dx*dx + dy*dy + dz*dz);

                                    if (distance <= outerRadius && distance >= innerRadius) {
                                        setVoxel(chunkData, x, y, z, true);
                                    }
                                }
                            }
                        }
                    }

                    // Generate mesh for this chunk
                    ChunkPosition chunkPos = {cx, cy, cz};
                    ChunkMesh chunkMesh = generateChunkMesh(chunkData, 0, chunkPos);  // textureIndex = 0 (stone texture)

                    // Skip empty chunks
                    if (chunkMesh.indices.empty()) continue;

                    // Store draw command for this chunk
                    VkDrawIndexedIndirectCommand cmd{};
                    cmd.indexCount = static_cast<uint32_t>(chunkMesh.indices.size());
                    cmd.instanceCount = 1;
                    cmd.firstIndex = static_cast<uint32_t>(allIndices.size());
                    cmd.vertexOffset = static_cast<int32_t>(allVertices.size());
                    cmd.firstInstance = 0;
                    drawCommands.push_back(cmd);

                    // Append vertices and indices
                    allVertices.insert(allVertices.end(), chunkMesh.vertices.begin(), chunkMesh.vertices.end());
                    allIndices.insert(allIndices.end(), chunkMesh.indices.begin(), chunkMesh.indices.end());

                    totalChunks++;
                }
            }
        }

        std::cout << "[Main] Generated " << totalChunks << " non-empty chunks" << std::endl;
        std::cout << "[Main] Total: " << allVertices.size() << " vertices, "
                  << allIndices.size() << " indices, "
                  << drawCommands.size() << " draw commands" << std::endl;

        // Use the combined mesh data
        auto& vertices = allVertices;
        auto& indices = allIndices;

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

        // Cleanup staging buffer (keep command pool for texture uploads)
        stagingBuffer.cleanup();

        // Create indirect draw buffer with all draw commands
        Buffer indirectBuffer;
        indirectBuffer.init(
            vulkanContext.getAllocator(),
            drawCommands.size() * sizeof(VkDrawIndexedIndirectCommand),
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
        );
        void* indirectData = indirectBuffer.map();
        memcpy(indirectData, drawCommands.data(), drawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
        indirectBuffer.unmap();

        std::cout << "[Main] Created multi-draw indirect buffer with " << drawCommands.size() << " draw commands" << std::endl;

        // Initialize bindless texture manager
        BindlessTextureManager textureManager;
        textureManager.init(vulkanContext.getDevice().getLogicalDevice(), vulkanContext.getAllocator(), 1024);

        // Load stone texture
        vkResetCommandBuffer(uploadCmd, 0);
        vkBeginCommandBuffer(uploadCmd, &beginInfo);
        uint32_t stoneTextureIndex = textureManager.loadTexture("assets/minecraft/textures/block/stone.png", uploadCmd, true);
        vkEndCommandBuffer(uploadCmd);
        vkQueueSubmit(vulkanContext.getDevice().getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(vulkanContext.getDevice().getGraphicsQueue());

        // Cleanup upload resources
        vkDestroyCommandPool(vulkanContext.getDevice().getLogicalDevice(), uploadPool, nullptr);

        // Create graphics pipeline with vertex input
        GraphicsPipelineConfig pipelineConfig;
        pipelineConfig.vertexShader = &vertShader;
        pipelineConfig.fragmentShader = &fragShader;
        pipelineConfig.colorFormat = swapchain.getImageFormat();
        pipelineConfig.depthFormat = depthFormat;
        pipelineConfig.depthTest = true;
        pipelineConfig.depthWrite = true; // Enable depth buffer writes
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

        // Add bindless texture descriptor set layout to pipeline config
        pipelineConfig.descriptorSetLayouts.push_back(textureManager.getDescriptorSetLayout());

        GraphicsPipeline pipeline;
        pipeline.init(vulkanContext.getDevice().getLogicalDevice(), pipelineConfig);

        // Create camera (position it to view the chunk grid)
        Camera camera;
        float aspectRatio = static_cast<float>(window.getWidth()) / static_cast<float>(window.getHeight());
        float gridCenter = (CHUNK_GRID_SIZE * CHUNK_SIZE) / 2.0f;
        camera.init(glm::vec3(gridCenter, gridCenter, gridCenter + 60.0f), aspectRatio, 70.0f);

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

                // Recreate swapchain
                swapchain.recreate(width, height);

                // Recreate depth buffer with new size
                vkDestroyImageView(vulkanContext.getDevice().getLogicalDevice(), depthImageView, nullptr);
                vmaDestroyImage(vulkanContext.getAllocator(), depthImage, depthAllocation);

                depthImageInfo.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
                vmaCreateImage(vulkanContext.getAllocator(), &depthImageInfo, &depthAllocInfo, &depthImage, &depthAllocation, nullptr);

                depthViewInfo.image = depthImage;
                vkCreateImageView(vulkanContext.getDevice().getLogicalDevice(), &depthViewInfo, nullptr, &depthImageView);

                framebufferResized = false;
                std::cout << "[Main] Swapchain and depth buffer recreated (" << width << "x" << height << ")" << std::endl;
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

            // Begin rendering to swapchain image with depth
            cmd.beginRendering(
                swapchain.getImageViews()[renderer.getCurrentImageIndex()],
                swapchain.getExtent(),
                glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), // Dark gray clear color
                depthImageView // Depth attachment
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

            // Bind bindless texture descriptor set
            VkDescriptorSet textureDescSet = textureManager.getDescriptorSet();
            cmd.bindDescriptorSets(pipeline.getLayout(), 0, 1, &textureDescSet);

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

            // Draw all chunks using multi-draw indirect
            cmd.drawIndexedIndirect(indirectBuffer.getBuffer(), 0, static_cast<uint32_t>(drawCommands.size()), sizeof(VkDrawIndexedIndirectCommand));

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