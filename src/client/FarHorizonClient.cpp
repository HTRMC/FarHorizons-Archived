#include "FarHorizonClient.hpp"
#include "render/RenderManager.hpp"
#include "render/TextureManager.hpp"
#include "core/Raycast.hpp"
#include "world/BlockRegistry.hpp"
#include <spdlog/spdlog.h>

namespace FarHorizon {

FarHorizonClient::FarHorizonClient()
    : running(false)
    , framebufferResized(false)
    , currentDeltaTime(0.0f)
    , fpsUpdateTimer(0.0f)
    , lastFps(0)
    , previousState(GameStateManager::State::MainMenu) {
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
    spdlog::info("  WASD - Move");
    spdlog::info("  Left Ctrl - Sprint");
    spdlog::info("  Mouse - Look around");
    spdlog::info("  Space - Jump (or fly up in NoClip)");
    spdlog::info("  Shift - Fly down (in NoClip)");
    spdlog::info("  F - Toggle NoClip");
    spdlog::info("  1-5 - Select blocks");
    spdlog::info("  Left Click - Break block");
    spdlog::info("  Right Click - Place block");
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

    // Initialize physics system
    level = std::make_unique<Level>(chunkManager.get());
    player = std::make_unique<Player>();
    player->setLevel(level.get()); // Set the level reference
    player->setPos(glm::dvec3(0.0, 100.0, 0.0)); // Start high up
    spdlog::info("Initialized physics system with collision detection");

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
        currentDeltaTime = std::chrono::duration<float>(currentTime - lastTime).count();

        lastTime = currentTime;

        // Update FPS counter once per second
        fpsUpdateTimer += currentDeltaTime;
        if (fpsUpdateTimer >= 1.0f) {
            lastFps = currentDeltaTime > 0.0f ? static_cast<int>(1.0f / currentDeltaTime) : 0;
            fpsUpdateTimer = 0.0f;
        }

        // Poll events and process input
        window->pollEvents();
        InputSystem::processEvents();

        // Update game state
        tick(currentDeltaTime);

        // Handle input
        handleInput(currentDeltaTime);

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
    // Update game state manager (still uses deltaTime for UI animations)
    bool shouldQuit = gameStateManager->update(deltaTime);
    if (shouldQuit) {
        running = false;
        return;
    }

    // Detect world reset (transition to MainMenu from any gameplay state)
    auto currentState = gameStateManager->getState();
    if ((previousState == GameStateManager::State::Playing ||
         previousState == GameStateManager::State::Paused ||
         previousState == GameStateManager::State::Options) &&
        currentState == GameStateManager::State::MainMenu) {
        spdlog::info("World reset detected, clearing GPU buffers and pending meshes");
        renderManager->clearChunkBuffers();
        pendingMeshes.clear();
    }
    previousState = currentState;

    // Handle texture reloading when mipmap settings change
    if (gameStateManager->needsTextureReload()) {
        spdlog::info("Mipmap settings changed - hot reloading all block textures...");
        gameStateManager->clearTextureReloadFlag();

        renderManager->waitIdle();

        VkCommandPoolCreateInfo reloadPoolInfo{};
        reloadPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        reloadPoolInfo.queueFamilyIndex = renderManager->getQueueFamilyIndices().graphicsFamily.value();
        reloadPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

        VkCommandPool reloadPool;
        vkCreateCommandPool(renderManager->getDevice(), &reloadPoolInfo, nullptr, &reloadPool);

        VkCommandBufferAllocateInfo reloadAllocInfo{};
        reloadAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        reloadAllocInfo.commandPool = reloadPool;
        reloadAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        reloadAllocInfo.commandBufferCount = 1;

        VkCommandBuffer reloadCmd;
        vkAllocateCommandBuffers(renderManager->getDevice(), &reloadAllocInfo, &reloadCmd);

        VkCommandBufferBeginInfo reloadBeginInfo{};
        reloadBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        reloadBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(reloadCmd, &reloadBeginInfo);

        auto requiredTextures = chunkManager->getRequiredTextures();
        std::set<std::string> textureSet(requiredTextures.begin(), requiredTextures.end());
        textureManager->reloadTextures(textureSet, *settings, reloadCmd);

        vkEndCommandBuffer(reloadCmd);

        VkSubmitInfo reloadSubmitInfo{};
        reloadSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        reloadSubmitInfo.commandBufferCount = 1;
        reloadSubmitInfo.pCommandBuffers = &reloadCmd;
        vkQueueSubmit(renderManager->getGraphicsQueue(), 1, &reloadSubmitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(renderManager->getGraphicsQueue());

        vkDestroyCommandPool(renderManager->getDevice(), reloadPool, nullptr);

        spdlog::info("Texture hot reload complete");
    }

    if (gameStateManager->isPlaying()) {
        // Minecraft's exact pattern: beginRenderTick returns number of ticks to runIs t
        int ticksToRun = tickManager.beginRenderTick(deltaTime, true);

        for (int i = 0; i < ticksToRun; ++i) {
            // Sample movement input and apply it ONCE per tick (not per frame!)
            // Minecraft's pattern: sidewaysSpeed (x), upwardSpeed (y), forwardSpeed (z)
            float forwardSpeed = 0.0f;
            float sidewaysSpeed = 0.0f;
            if (InputSystem::isKeyPressed(KeyCode::W)) forwardSpeed += 1.0f;
            if (InputSystem::isKeyPressed(KeyCode::S)) forwardSpeed -= 1.0f;
            if (InputSystem::isKeyPressed(KeyCode::A)) sidewaysSpeed -= 1.0f;
            if (InputSystem::isKeyPressed(KeyCode::D)) sidewaysSpeed += 1.0f;

            // Set movement input and yaw for this tick
            player->setMovementInput(forwardSpeed, sidewaysSpeed);
            player->setXRot(camera->getYaw()); // Entity's yaw is in degrees

            // Handle player physics and movement (at fixed 20 ticks/second)
            player->tick(level.get());
        }

        // Make camera follow player's eye position with sub-tick interpolation
        // This provides buttery-smooth rendering at high framerates (60Hz+) while physics runs at 20Hz
        // Uses Minecraft's exact pattern: lerp between lastRenderPos and pos using partialTick
        float partialTick = tickManager.getTickProgress();
        glm::dvec3 interpolatedEyePos = player->getLerpedEyePos(partialTick);
        camera->setPosition(glm::vec3(interpolatedEyePos));

        // Update chunks around player
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

    // Handle mouse look (camera rotation only)
    auto* mouseCapture = window->getMouseCapture();
    if (mouseCapture && mouseCapture->isCursorLocked()) {
        glm::vec2 mouseDelta(
            static_cast<float>(mouseCapture->getCursorDeltaX()),
            static_cast<float>(mouseCapture->getCursorDeltaY())
        );

        if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
            // Apply Minecraft's mouse sensitivity formula
            float d = settings->mouseSensitivity * 0.6f + 0.2f;
            float e = d * d * d;
            float f = e * 8.0f;

            float yawDelta = mouseDelta.x * f * 0.15f;
            float pitchDelta = -mouseDelta.y * f * 0.15f;
            camera->rotate(yawDelta, pitchDelta);
        }

        mouseCapture->resetDeltas();
    }

    // NOTE: WASD movement input is now handled in tick() loop (once per tick, not per frame)
    // Only handle vertical movement and other non-tick actions here

    // Handle jump input (Minecraft's pattern - set every frame, checked every tick)
    // This prevents timing issues between high FPS input and 20Hz physics
    if (player->isNoClip()) {
        // In noclip mode, Space/Shift = up/down
        auto vel = player->getVelocity();
        if (InputSystem::isKeyPressed(KeyCode::Space)) {
            vel.y = 10.0; // Fly up
        } else if (InputSystem::isKeyPressed(KeyCode::LeftShift) || InputSystem::isKeyPressed(KeyCode::RightShift)) {
            vel.y = -10.0; // Fly down
        } else {
            vel.y = 0.0; // Stop vertical movement
        }
        player->setVelocity(vel);
    } else {
        // In physics mode, set jumping flag (will be checked during physics tick)
        player->setJumping(InputSystem::isKeyPressed(KeyCode::Space));
    }

    // Toggle noclip with F (for testing)
    if (InputSystem::isKeyDown(KeyCode::F)) {
        player->setNoClip(!player->isNoClip());
        spdlog::info("NoClip: {}", player->isNoClip() ? "ON" : "OFF");
    }

    // Sprint handling (Minecraft LocalPlayer.java line 757-779)
    if (!player->isNoClip()) {
        bool wantsSprint = InputSystem::isKeyPressed(KeyCode::LeftControl);
        bool movingForward = InputSystem::isKeyPressed(KeyCode::W);

        // Start sprinting (line 766-768)
        if (wantsSprint && movingForward && !player->isSprinting()) {
            player->setSprinting(true);
        }

        // Stop sprinting (line 771-778: shouldStopRunSprinting)
        // Stops when: not moving forward OR hitting wall hard
        if (player->isSprinting()) {
            if (!movingForward || (player->horizontalCollision_ && !player->minorHorizontalCollision_)) {
                player->setSprinting(false);
            }
        }
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
    if (InputSystem::isKeyDown(KeyCode::Five)) {
        selectedBlock = BlockRegistry::GLASS;
        spdlog::info("Selected: Glass");
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
                             *settings, *textureManager, crosshairTarget, lastFps);
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