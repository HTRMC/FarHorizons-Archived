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

using namespace VoxelEngine;

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

        // Create graphics pipeline
        GraphicsPipelineConfig pipelineConfig;
        pipelineConfig.vertexShader = &vertShader;
        pipelineConfig.fragmentShader = &fragShader;
        pipelineConfig.colorFormat = swapchain.getImageFormat();
        pipelineConfig.depthTest = false;
        pipelineConfig.cullMode = VK_CULL_MODE_NONE; // Show both sides of triangle

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

            // Draw triangle
            cmd.draw(3); // 3 vertices (hardcoded in shader)

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