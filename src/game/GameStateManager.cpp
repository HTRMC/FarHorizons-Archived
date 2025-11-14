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
    : state_(State::MainMenu)
    , blockInputThisFrame_(false)
    , mainMenu_(windowWidth, windowHeight, settings)
    , pauseMenu_(windowWidth, windowHeight, settings)
    , optionsMenu_(windowWidth, windowHeight, camera, chunkManager, settings, audioManager)
    , mouseCapture_(mouseCapture)
    , camera_(camera)
    , chunkManager_(chunkManager)
    , settings_(settings)
    , audioManager_(audioManager)
    , aspectRatio_(static_cast<float>(windowWidth) / static_cast<float>(windowHeight))
{
    // Start in main menu with cursor unlocked
    mouseCapture_->unlockCursor();
}

bool GameStateManager::update(float deltaTime) {
    switch (state_) {
        case State::MainMenu: {
            auto action = mainMenu_.update(deltaTime);
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
            auto action = pauseMenu_.update(deltaTime);
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
            auto action = optionsMenu_.update(deltaTime);
            if (action == OptionsMenu::Action::Back) {
                setState(State::Paused);
                spdlog::info("Returning to pause menu");
            }
            // Update chunk manager to apply render distance changes
            chunkManager_->update(camera_->getPosition());
            return false;
        }

        case State::OptionsFromMain: {
            auto action = optionsMenu_.update(deltaTime);
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
    aspectRatio_ = static_cast<float>(width) / static_cast<float>(height);
    mainMenu_.onResize(width, height);
    pauseMenu_.onResize(width, height);
    optionsMenu_.onResize(width, height);
}

void GameStateManager::openPauseMenu() {
    if (state_ == State::Playing) {
        setState(State::Paused);
    }
}

bool GameStateManager::needsTextureReload() const {
    return optionsMenu_.needsTextureReload();
}

void GameStateManager::clearTextureReloadFlag() {
    optionsMenu_.clearTextureReloadFlag();
}

void GameStateManager::setState(State newState) {
    State oldState = state_;
    state_ = newState;

    // Handle cursor lock/unlock based on state transitions
    if (newState == State::Playing) {
        mouseCapture_->lockCursor();
        // Block input for one frame when transitioning to playing to prevent
        // menu clicks from being processed as gameplay actions
        if (oldState != State::Playing) {
            blockInputThisFrame_ = true;
        }
    } else if (oldState == State::Playing) {
        mouseCapture_->unlockCursor();
    }

    // Reset menus when entering them
    if (newState == State::Paused) {
        pauseMenu_.reset();
    } else if (newState == State::Options || newState == State::OptionsFromMain) {
        optionsMenu_.reset();
    }
}

void GameStateManager::resetWorld() {
    // Clear world state
    chunkManager_->clearAllChunks();

    // Reset camera to spawn position (preserve FOV, keybinds, and mouse sensitivity from settings)
    camera_->init(glm::vec3(0.0f, 20.0f, 0.0f), aspectRatio_, settings_->fov);
    camera_->setKeybinds(settings_->keybinds);
    camera_->setMouseSensitivity(settings_->mouseSensitivity);

    // Reset main menu
    mainMenu_.reset();
}

} // namespace FarHorizon