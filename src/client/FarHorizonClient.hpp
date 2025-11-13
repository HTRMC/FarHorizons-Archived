#pragma once

#include <memory>
#include <chrono>
#include <vector>
#include "core/Window.hpp"
#include "core/InputSystem.hpp"
#include "core/Camera.hpp"
#include "core/Settings.hpp"
#include "core/TickManager.hpp"
#include "world/ChunkManager.hpp"
#include "world/ChunkGpuData.hpp"
#include "audio/AudioManager.hpp"
#include "game/GameStateManager.hpp"
#include "game/InteractionManager.hpp"
#include "render/RenderManager.hpp"
#include "render/TextureManager.hpp"
#include "physics/Player.hpp"
#include "world/Level.hpp"

namespace FarHorizon {

/**
 * Far Horizon - Main client class.
 * This is the central hub that owns and coordinates all subsystems.
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

    // Getters for managers
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

    // Physics
    std::unique_ptr<Player> player;
    std::unique_ptr<Level> level;

    // Managers
    std::unique_ptr<RenderManager> renderManager;
    std::unique_ptr<TextureManager> textureManager;
    std::unique_ptr<AudioManager> audioManager;
    std::unique_ptr<ChunkManager> chunkManager;
    std::unique_ptr<GameStateManager> gameStateManager;
    std::unique_ptr<InteractionManager> interactionManager;

    // Timing
    std::chrono::high_resolution_clock::time_point lastTime;
    float currentDeltaTime;
    float fpsUpdateTimer;
    int lastFps;
    bool running;
    bool framebufferResized;
    TickManager tickManager;

    // Chunk mesh management
    std::vector<CompactChunkMesh> pendingMeshes;

    // State tracking for world reset detection
    GameStateManager::State previousState;
};

} // namespace FarHorizon