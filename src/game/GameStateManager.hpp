#pragma once

#include <memory>
#include "../ui/MainMenu.hpp"
#include "../ui/PauseMenu.hpp"
#include "../ui/OptionsMenu.hpp"
#include "../core/MouseCapture.hpp"
#include "../core/Camera.hpp"
#include "../core/Settings.hpp"
#include "../world/ChunkManager.hpp"
#include "../audio/AudioManager.hpp"

namespace FarHorizon {

/**
 * Manages game states and menu transitions.
 * Handles state flow between main menu, gameplay, pause, and options.
 */
class GameStateManager {
public:
    enum class State {
        MainMenu,
        Playing,
        Paused,
        Options,
        OptionsFromMain
    };

    /**
     * Initialize the game state manager with required dependencies.
     */
    GameStateManager(
        uint32_t windowWidth,
        uint32_t windowHeight,
        MouseCapture* mouseCapture,
        Camera* camera,
        ChunkManager* chunkManager,
        Settings* settings,
        AudioManager* audioManager
    );

    /**
     * Update current state and handle menu logic.
     * Returns true if the application should quit.
     */
    bool update(float deltaTime);

    /**
     * Handle window resize events.
     */
    void onResize(uint32_t width, uint32_t height);

    /**
     * Get current game state.
     */
    State getState() const { return m_state; }

    /**
     * Check if currently in gameplay (not in menus).
     */
    bool isPlaying() const { return m_state == State::Playing; }

    /**
     * Request to open pause menu (from gameplay).
     */
    void openPauseMenu();

    /**
     * Get menu instances for rendering.
     */
    MainMenu& getMainMenu() { return m_mainMenu; }
    PauseMenu& getPauseMenu() { return m_pauseMenu; }
    OptionsMenu& getOptionsMenu() { return m_optionsMenu; }

    /**
     * Check if texture reload is needed (mipmap settings changed).
     */
    bool needsTextureReload() const;

    /**
     * Clear texture reload flag after reloading.
     */
    void clearTextureReloadFlag();

private:
    State m_state;

    // Menus
    MainMenu m_mainMenu;
    PauseMenu m_pauseMenu;
    OptionsMenu m_optionsMenu;

    // Dependencies
    MouseCapture* m_mouseCapture;
    Camera* m_camera;
    ChunkManager* m_chunkManager;
    Settings* m_settings;
    AudioManager* m_audioManager;

    // For world reset
    float m_aspectRatio;

    /**
     * Transition to a new state.
     */
    void setState(State newState);

    /**
     * Reset world state when returning to main menu.
     */
    void resetWorld();
};

} // namespace FarHorizon