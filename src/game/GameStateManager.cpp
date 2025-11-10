#include "GameStateManager.hpp"
#include <spdlog/spdlog.h>

namespace FarHorizon {

GameStateManager::GameStateManager(
    uint32_t windowWidth,
    uint32_t windowHeight,
    MouseCapture* mouseCapture,
    Camera* camera,
    ChunkManager* chunkManager,
    Settings* settings,
    AudioManager* audioManager
)
    : m_state(State::MainMenu)
    , m_mainMenu(windowWidth, windowHeight)
    , m_pauseMenu(windowWidth, windowHeight, settings)
    , m_optionsMenu(windowWidth, windowHeight, camera, chunkManager, settings, audioManager)
    , m_mouseCapture(mouseCapture)
    , m_camera(camera)
    , m_chunkManager(chunkManager)
    , m_settings(settings)
    , m_audioManager(audioManager)
    , m_aspectRatio(static_cast<float>(windowWidth) / static_cast<float>(windowHeight))
{
    // Start in main menu with cursor unlocked
    m_mouseCapture->unlockCursor();
}

bool GameStateManager::update(float deltaTime) {
    switch (m_state) {
        case State::MainMenu: {
            auto action = m_mainMenu.update(deltaTime);
            switch (action) {
                case MainMenu::Action::Singleplayer:
                    setState(State::Playing);
                    spdlog::info("Starting singleplayer game");
                    return false;
                case MainMenu::Action::OpenOptions:
                    setState(State::OptionsFromMain);
                    spdlog::info("Opening options menu from main menu");
                    return false;
                case MainMenu::Action::Quit:
                    return true;  // Quit application
                case MainMenu::Action::None:
                    return false;
            }
            break;
        }

        case State::Playing:
            // Playing state is handled in main loop (camera, world updates, interactions)
            // This manager only handles transitions away from playing state
            return false;

        case State::Paused: {
            auto action = m_pauseMenu.update(deltaTime);
            switch (action) {
                case PauseMenu::Action::Resume:
                    setState(State::Playing);
                    return false;
                case PauseMenu::Action::OpenOptions:
                    setState(State::Options);
                    spdlog::info("Opening options menu from pause menu");
                    return false;
                case PauseMenu::Action::Quit:
                    resetWorld();
                    setState(State::MainMenu);
                    spdlog::info("Returning to main menu");
                    return false;
                case PauseMenu::Action::None:
                    return false;
            }
            break;
        }

        case State::Options: {
            auto action = m_optionsMenu.update(deltaTime);
            if (action == OptionsMenu::Action::Back) {
                setState(State::Paused);
                spdlog::info("Returning to pause menu");
            }
            // Update chunk manager to apply render distance changes
            m_chunkManager->update(m_camera->getPosition());
            return false;
        }

        case State::OptionsFromMain: {
            auto action = m_optionsMenu.update(deltaTime);
            if (action == OptionsMenu::Action::Back) {
                setState(State::MainMenu);
                spdlog::info("Returning to main menu");
            }
            // Don't update chunk manager - game hasn't started yet
            return false;
        }
    }

    return false;
}

void GameStateManager::onResize(uint32_t width, uint32_t height) {
    m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    m_mainMenu.onResize(width, height);
    m_pauseMenu.onResize(width, height);
    m_optionsMenu.onResize(width, height);
}

void GameStateManager::openPauseMenu() {
    if (m_state == State::Playing) {
        setState(State::Paused);
    }
}

bool GameStateManager::needsTextureReload() const {
    return m_optionsMenu.needsTextureReload();
}

void GameStateManager::clearTextureReloadFlag() {
    m_optionsMenu.clearTextureReloadFlag();
}

void GameStateManager::setState(State newState) {
    State oldState = m_state;
    m_state = newState;

    // Handle cursor lock/unlock based on state transitions
    if (newState == State::Playing) {
        m_mouseCapture->lockCursor();
    } else if (oldState == State::Playing) {
        m_mouseCapture->unlockCursor();
    }

    // Reset menus when entering them
    if (newState == State::Paused) {
        m_pauseMenu.reset();
    } else if (newState == State::Options || newState == State::OptionsFromMain) {
        m_optionsMenu.reset();
    }
}

void GameStateManager::resetWorld() {
    // Clear world state
    m_chunkManager->clearAllChunks();

    // Reset camera to spawn position (preserve FOV, keybinds, and mouse sensitivity from settings)
    m_camera->init(glm::vec3(0.0f, 20.0f, 0.0f), m_aspectRatio, m_settings->fov);
    m_camera->setKeybinds(m_settings->keybinds);
    m_camera->setMouseSensitivity(m_settings->mouseSensitivity);

    // Reset main menu
    m_mainMenu.reset();
}

} // namespace FarHorizon