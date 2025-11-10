#include "FarHorizonClient.hpp"
#include "render/RenderManager.hpp"
#include "render/TextureManager.hpp"
#include "core/Raycast.hpp"
#include "world/BlockRegistry.hpp"
#include <spdlog/spdlog.h>

namespace FarHorizon {

FarHorizonClient::FarHorizonClient()
    : running(false)
    , framebufferResized(false) {
}

FarHorizonClient::~FarHorizonClient() = default;

void FarHorizonClient::init() {
    spdlog::info("Initializing Far Horizon...");

    // Create window
    WindowProperties props;
    props.title = "Far Horizon - Infinite Voxel Engine";
    props.width = 1600;
    props.height = 900;
    props.vsync = true;
    props.resizable = true;

    window = std::make_unique<Window>(props);
    InputSystem::init(window->getNativeWindow());

    // Initialize mouse capture
    auto* mouseCapture = window->getMouseCapture();
    InputSystem::setMouseCapture(mouseCapture);

    spdlog::info("=== Far Horizon - Infinite Voxel Engine ===");
    spdlog::info("Controls:");
    spdlog::info("  WASD - Move camera");
    spdlog::info("  Mouse - Rotate camera");
    spdlog::info("  Space/Shift - Move up/down");
    spdlog::info("  ESC - Pause menu");
    spdlog::info("==========================================");

    // Load settings
    settings = std::make_unique<Settings>();
    settings->load();

    // Initialize block registry
    BlockRegistry::init();
    spdlog::info("Initialized block registry");

    // Initialize audio manager
    audioManager = std::make_unique<AudioManager>();
    audioManager->init();
    audioManager->loadSoundsFromJson("assets/minecraft/sounds.json");
    audioManager->setMasterVolume(settings->masterVolume.getValue());

    // Initialize chunk manager
    chunkManager = std::make_unique<ChunkManager>();
    chunkManager->setRenderDistance(settings->renderDistance);
    chunkManager->initializeBlockModels();
    chunkManager->preloadBlockStateModels();
    chunkManager->precacheBlockShapes();

    // Initialize rendering systems
    renderManager = std::make_unique<RenderManager>();
    textureManager = std::make_unique<TextureManager>();

    renderManager->init(window->getNativeWindow(), *textureManager);

    VkCommandPoolCreateInfo texPoolInfo{};
    texPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    texPoolInfo.queueFamilyIndex = renderManager->getQueueFamilyIndices().graphicsFamily.value();
    texPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    VkCommandPool texPool;
    vkCreateCommandPool(renderManager->getDevice(), &texPoolInfo, nullptr, &texPool);

    VkCommandBufferAllocateInfo texAllocInfo{};
    texAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    texAllocInfo.commandPool = texPool;
    texAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    texAllocInfo.commandBufferCount = 1;

    VkCommandBuffer texCmd;
    vkAllocateCommandBuffers(renderManager->getDevice(), &texAllocInfo, &texCmd);

    VkCommandBufferBeginInfo texBeginInfo{};
    texBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    texBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(texCmd, &texBeginInfo);

    textureManager->loadBlockTextures(*chunkManager, *settings, texCmd);
    textureManager->loadFonts(texCmd);

    vkEndCommandBuffer(texCmd);

    VkSubmitInfo texSubmitInfo{};
    texSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    texSubmitInfo.commandBufferCount = 1;
    texSubmitInfo.pCommandBuffers = &texCmd;
    vkQueueSubmit(renderManager->getGraphicsQueue(), 1, &texSubmitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(renderManager->getGraphicsQueue());

    vkDestroyCommandPool(renderManager->getDevice(), texPool, nullptr);

    // Initialize camera
    camera = std::make_unique<Camera>();
    float aspectRatio = static_cast<float>(window->getWidth()) / static_cast<float>(window->getHeight());
    camera->init(glm::vec3(0.0f, 20.0f, 0.0f), aspectRatio, settings->fov);
    camera->setKeybinds(settings->keybinds);
    camera->setMouseSensitivity(settings->mouseSensitivity);
    camera->setMouseCapture(mouseCapture);

    // Initialize game state manager
    gameStateManager = std::make_unique<GameStateManager>(
        window->getWidth(), window->getHeight(),
        mouseCapture, camera.get(), chunkManager.get(), settings.get(), audioManager.get()
    );

    // Initialize interaction manager
    interactionManager = std::make_unique<InteractionManager>(*chunkManager, *audioManager);

    // Setup resize callback
    window->setResizeCallback([this](uint32_t width, uint32_t height) {
        framebufferResized = true;
        camera->setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
        gameStateManager->onResize(width, height);
    });

    spdlog::info("Far Horizon initialization complete");
}

void FarHorizonClient::run() {
    running = true;
    lastTime = std::chrono::high_resolution_clock::now();

    spdlog::info("Entering main loop...");

    while (!window->shouldClose() && running) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Poll events and process input
        window->pollEvents();
        InputSystem::processEvents();

        // Update game state
        tick(deltaTime);

        // Handle input
        handleInput(deltaTime);

        // Handle resize
        if (framebufferResized) {
            handleResize();
        }

        // Render
        render();
    }

    renderManager->waitIdle();
}

void FarHorizonClient::tick(float deltaTime) {
    // Update game state manager
    bool shouldQuit = gameStateManager->update(deltaTime);
    if (shouldQuit) {
        running = false;
        return;
    }

    // Update world and camera when playing
    if (gameStateManager->isPlaying()) {
        camera->update(deltaTime);
        chunkManager->update(camera->getPosition());

        // Collect ready meshes from chunk manager
        if (chunkManager->hasReadyMeshes()) {
            auto readyMeshes = chunkManager->getReadyMeshes();
            pendingMeshes.insert(pendingMeshes.end(),
                                std::make_move_iterator(readyMeshes.begin()),
                                std::make_move_iterator(readyMeshes.end()));
        }

        auto& bufferManager = renderManager->getChunkBufferManager();

        // Remove unloaded chunks
        bufferManager.removeUnloadedChunks(*chunkManager);

        // Check for compaction
        bufferManager.compactIfNeeded(bufferManager.getMeshCache());

        // Add pending meshes incrementally (up to 20 per frame)
        if (!pendingMeshes.empty()) {
            bufferManager.addMeshes(pendingMeshes, 20);
            size_t processCount = std::min(pendingMeshes.size(), size_t(20));
            pendingMeshes.erase(pendingMeshes.begin(), pendingMeshes.begin() + processCount);
            renderManager->markQuadInfoForUpdate();
        }
    }
}

void FarHorizonClient::handleInput(float deltaTime) {
    if (!gameStateManager->isPlaying()) {
        return;  // Don't handle gameplay input when not playing
    }

    // ESC to pause
    if (InputSystem::isKeyDown(KeyCode::Escape)) {
        gameStateManager->openPauseMenu();
        return;
    }

    // Block selection with number keys
    static Block* selectedBlock = BlockRegistry::STONE;
    if (InputSystem::isKeyDown(KeyCode::One)) {
        selectedBlock = BlockRegistry::STONE;
        spdlog::info("Selected: Stone");
    }
    if (InputSystem::isKeyDown(KeyCode::Two)) {
        selectedBlock = BlockRegistry::STONE_SLAB;
        spdlog::info("Selected: Stone Slab");
    }
    if (InputSystem::isKeyDown(KeyCode::Three)) {
        selectedBlock = BlockRegistry::GRASS_BLOCK;
        spdlog::info("Selected: Grass Block");
    }
    if (InputSystem::isKeyDown(KeyCode::Four)) {
        selectedBlock = BlockRegistry::OAK_STAIRS;
        spdlog::info("Selected: Oak Stairs");
    }

    // Raycast for block interaction
    auto crosshairTarget = Raycast::castRay(*chunkManager, camera->getPosition(),
                                           camera->getForward(), 8.0f);

    // Block breaking (left click)
    if (InputSystem::isMouseButtonDown(MouseButton::Left)) {
        if (crosshairTarget.has_value()) {
            interactionManager->breakBlock(crosshairTarget.value());
        }
    }

    // Block placing (right click)
    if (InputSystem::isMouseButtonDown(MouseButton::Right)) {
        if (crosshairTarget.has_value()) {
            interactionManager->placeBlock(crosshairTarget.value(), selectedBlock,
                                         camera->getForward());
        }
    }
}

void FarHorizonClient::render() {
    // Perform raycast for crosshair target
    std::optional<BlockHitResult> crosshairTarget;
    if (gameStateManager->isPlaying()) {
        crosshairTarget = Raycast::castRay(*chunkManager, camera->getPosition(),
                                          camera->getForward(), 8.0f);
    }

    // Render the frame
    if (renderManager->beginFrame()) {
        renderManager->render(*camera, *chunkManager, *gameStateManager,
                             *settings, *textureManager, crosshairTarget);
        renderManager->endFrame();
    } else {
        // Swapchain recreation needed
        handleResize();
    }
}

void FarHorizonClient::handleResize() {
    int width = window->getWidth();
    int height = window->getHeight();

    while (width == 0 || height == 0) {
        window->pollEvents();
        width = window->getWidth();
        height = window->getHeight();
    }

    renderManager->onResize(width, height, *textureManager);
    framebufferResized = false;
}

void FarHorizonClient::shutdown() {
    spdlog::info("Shutting down Far Horizon...");

    // Cleanup in reverse order of initialization
    interactionManager.reset();
    gameStateManager.reset();
    camera.reset();

    textureManager->shutdown();
    textureManager.reset();

    renderManager.reset();

    chunkManager.reset();

    audioManager->cleanup();
    audioManager.reset();

    settings.reset();

    InputSystem::shutdown();
    window.reset();

    spdlog::info("Shutdown complete");
}

} // namespace FarHorizon