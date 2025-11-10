#pragma once

#include <memory>
#include <chrono>
#include "core/Window.hpp"
#include "core/InputSystem.hpp"
#include "core/Camera.hpp"
#include "core/Settings.hpp"
#include "world/ChunkManager.hpp"
#include "audio/AudioManager.hpp"
#include "game/GameStateManager.hpp"
#include "game/InteractionManager.hpp"

namespace FarHorizon {

// Forward declarations
class RenderManager;
class TextureManager;

/**
 * Far Horizon - Main client class following Minecraft's architecture pattern.
 * This is the central hub that owns and coordinates all subsystems.
 * Similar to net.minecraft.client.MinecraftClient
 */
class FarHorizonClient {
public:
    FarHorizonClient();
    ~FarHorizonClient();

    // No copy/move
    FarHorizonClient(const FarHorizonClient&) = delete;
    FarHorizonClient& operator=(const FarHorizonClient&) = delete;

    /**
     * Initialize all subsystems
     */
    void init();

    /**
     * Main game loop - runs until window closes
     */
    void run();

    /**
     * Shutdown and cleanup all subsystems
     */
    void shutdown();

    // Getters for managers (following Minecraft pattern)
    Window& getWindow() { return *window; }
    Camera& getCamera() { return *camera; }
    ChunkManager& getChunkManager() { return *chunkManager; }
    AudioManager& getAudioManager() { return *audioManager; }
    Settings& getSettings() { return *settings; }

private:
    void tick(float deltaTime);
    void render();
    void handleInput(float deltaTime);
    void handleResize();

private:
    // Core systems
    std::unique_ptr<Window> window;
    std::unique_ptr<Camera> camera;
    std::unique_ptr<Settings> settings;

    // Managers (following Minecraft's manager pattern)
    std::unique_ptr<RenderManager> renderManager;
    std::unique_ptr<TextureManager> textureManager;
    std::unique_ptr<AudioManager> audioManager;
    std::unique_ptr<ChunkManager> chunkManager;
    std::unique_ptr<GameStateManager> gameStateManager;
    std::unique_ptr<InteractionManager> interactionManager;

    // Timing
    std::chrono::high_resolution_clock::time_point lastTime;
    bool running;
    bool framebufferResized;
};

} // namespace FarHorizon