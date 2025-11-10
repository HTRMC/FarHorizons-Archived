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

    // Create upload command pool for texture loading
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = renderManager->getQueueFamilyIndices().graphicsFamily.value();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPool uploadPool;
    vkCreateCommandPool(renderManager->getDevice(), &poolInfo, nullptr, &uploadPool);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = uploadPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer uploadCmd;
    vkAllocateCommandBuffers(renderManager->getDevice(), &allocInfo, &uploadCmd);

    // Initialize texture manager
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(uploadCmd, &beginInfo);

    textureManager->init(renderManager->getDevice(), renderManager->getAllocator(), uploadCmd);
    textureManager->loadBlockTextures(*chunkManager, *settings, uploadCmd);
    textureManager->loadFonts(uploadCmd);

    vkEndCommandBuffer(uploadCmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &uploadCmd;
    vkQueueSubmit(renderManager->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(renderManager->getGraphicsQueue());

    // Cleanup upload resources (we're keeping a persistent pool in actual implementation)
    vkDestroyCommandPool(renderManager->getDevice(), uploadPool, nullptr);

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
            // Add to render manager's buffer manager
            // (Full implementation would handle this properly)
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