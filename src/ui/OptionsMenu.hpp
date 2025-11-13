#pragma once

#include "Button.hpp"
#include "Slider.hpp"
#include "CyclingButton.hpp"
#include "Panel.hpp"
#include "../text/TextRenderer.hpp"
#include "../core/InputSystem.hpp"
#include "../core/Camera.hpp"
#include "../core/Settings.hpp"
#include "../core/KeybindAction.hpp"
#include "../world/ChunkManager.hpp"
#include "../audio/AudioManager.hpp"
#include <memory>
#include <vector>
#include <optional>
#include <magic_enum/magic_enum.hpp>

namespace FarHorizon {

/**
 * Options menu UI with FOV, render distance sliders, and keybind configuration.
 * Allows real-time adjustment of game settings.
 */
class OptionsMenu {
public:
    enum class Action {
        None,
        Back
    };

    OptionsMenu(uint32_t screenWidth, uint32_t screenHeight, Camera* camera, ChunkManager* chunkManager, Settings* settings, AudioManager* audioManager = nullptr)
        : screenWidth_(screenWidth)
        , screenHeight_(screenHeight)
        , camera_(camera)
        , chunkManager_(chunkManager)
        , settings_(settings)
        , audioManager_(audioManager)
        , selectedButtonIndex_(0)
        , lastAction_(Action::None)
        , mouseWasDown_(false)
        , scrollOffset_(0.0f)
        , texturesNeedReload_(false) {
        setupUI();
    }

    // Check if textures need to be reloaded (e.g., mipmap level changed)
    bool needsTextureReload() const { return texturesNeedReload_; }
    void clearTextureReloadFlag() { texturesNeedReload_ = false; }

    // Update menu state with input
    Action update(float deltaTime) {
        lastAction_ = Action::None;

        // If listening for keybind, wait for any key press
        if (listeningForKeybind_.has_value()) {
            // Special handling for modifier keys (they don't always trigger isKeyDown)
            // Check if any modifiers are currently pressed
            KeyCode detectedModifier = KeyCode::Unknown;
            if (InputSystem::isKeyPressed(KeyCode::LeftShift) && !lastModifierState_[0]) {
                detectedModifier = KeyCode::LeftShift;
                lastModifierState_[0] = true;
            } else if (InputSystem::isKeyPressed(KeyCode::RightShift) && !lastModifierState_[1]) {
                detectedModifier = KeyCode::RightShift;
                lastModifierState_[1] = true;
            } else if (InputSystem::isKeyPressed(KeyCode::LeftControl) && !lastModifierState_[2]) {
                detectedModifier = KeyCode::LeftControl;
                lastModifierState_[2] = true;
            } else if (InputSystem::isKeyPressed(KeyCode::RightControl) && !lastModifierState_[3]) {
                detectedModifier = KeyCode::RightControl;
                lastModifierState_[3] = true;
            } else if (InputSystem::isKeyPressed(KeyCode::LeftAlt) && !lastModifierState_[4]) {
                detectedModifier = KeyCode::LeftAlt;
                lastModifierState_[4] = true;
            } else if (InputSystem::isKeyPressed(KeyCode::RightAlt) && !lastModifierState_[5]) {
                detectedModifier = KeyCode::RightAlt;
                lastModifierState_[5] = true;
            }

            // Reset modifier states when released
            if (!InputSystem::isKeyPressed(KeyCode::LeftShift)) lastModifierState_[0] = false;
            if (!InputSystem::isKeyPressed(KeyCode::RightShift)) lastModifierState_[1] = false;
            if (!InputSystem::isKeyPressed(KeyCode::LeftControl)) lastModifierState_[2] = false;
            if (!InputSystem::isKeyPressed(KeyCode::RightControl)) lastModifierState_[3] = false;
            if (!InputSystem::isKeyPressed(KeyCode::LeftAlt)) lastModifierState_[4] = false;
            if (!InputSystem::isKeyPressed(KeyCode::RightAlt)) lastModifierState_[5] = false;

            if (detectedModifier != KeyCode::Unknown) {
                // Rebind to the modifier key
                std::string keyName = std::string(magic_enum::enum_name(detectedModifier));

                spdlog::debug("Detected modifier: {}, keyName: '{}'", static_cast<int>(detectedModifier), keyName);

                // Convert to settings format
                std::string settingsKey = "key.keyboard.";
                bool lastWasLower = false;
                for (size_t i = 0; i < keyName.size(); i++) {
                    if (std::isupper(keyName[i]) && i > 0 && lastWasLower) {
                        settingsKey += '.';
                    }
                    settingsKey += std::tolower(keyName[i]);
                    lastWasLower = std::islower(keyName[i]);
                }

                spdlog::debug("Converted to settingsKey: '{}'", settingsKey);

                // Update settings
                if (settings_) {
                    std::string actionKey = keybindActionToString(listeningForKeybind_.value());
                    settings_->keybinds[actionKey] = settingsKey;
                    settings_->save();

                    if (camera_) {
                        camera_->setKeybinds(settings_->keybinds);
                    }

                    spdlog::info("Rebound {} to {}", actionKey, settingsKey);
                }

                setupUI();
                listeningForKeybind_.reset();
                return lastAction_;
            }

            // Check all other keys using magic_enum
            for (auto keyCode : magic_enum::enum_values<KeyCode>()) {
                if (keyCode == KeyCode::Unknown || keyCode == KeyCode::MaxKeys) continue;
                if (keyCode == KeyCode::Escape) continue; // Don't allow ESC to be bound

                // Skip modifier keys (handled above)
                if (keyCode == KeyCode::LeftShift || keyCode == KeyCode::RightShift ||
                    keyCode == KeyCode::LeftControl || keyCode == KeyCode::RightControl ||
                    keyCode == KeyCode::LeftAlt || keyCode == KeyCode::RightAlt) {
                    continue;
                }

                // isKeyDown detects when key just went down (edge trigger)
                if (InputSystem::isKeyDown(keyCode)) {
                    // Rebind the key
                    std::string keyName = std::string(magic_enum::enum_name(keyCode));

                    // Convert to settings format (e.g., "W" -> "key.keyboard.w")
                    std::string settingsKey = "key.keyboard.";

                    // Handle special cases with multiple words (e.g., "LeftShift" -> "left.shift")
                    bool lastWasLower = false;
                    for (size_t i = 0; i < keyName.size(); i++) {
                        if (std::isupper(keyName[i]) && i > 0 && lastWasLower) {
                            settingsKey += '.';
                        }
                        settingsKey += std::tolower(keyName[i]);
                        lastWasLower = std::islower(keyName[i]);
                    }

                    // Update settings
                    if (settings_) {
                        std::string actionKey = keybindActionToString(listeningForKeybind_.value());
                        settings_->keybinds[actionKey] = settingsKey;
                        settings_->save();

                        // Apply keybinds to camera
                        if (camera_) {
                            camera_->setKeybinds(settings_->keybinds);
                        }

                        spdlog::info("Rebound {} to {}", actionKey, settingsKey);
                    }

                    // Rebuild UI to show new keybind
                    setupUI();
                    listeningForKeybind_.reset();
                    return lastAction_;
                }
            }

            // Allow ESC to cancel rebinding
            if (InputSystem::isKeyDown(KeyCode::Escape)) {
                listeningForKeybind_.reset();
                spdlog::info("Cancelled keybind");
                return lastAction_;
            }

            return lastAction_; // Don't process other input while listening
        }

        // Handle mouse wheel scrolling
        auto mouseScroll = InputSystem::getMouseScroll();
        if (mouseScroll.y != 0.0f) {
            // Calculate effective GUI scale for scroll speed
            float guiScale = settings_ ? static_cast<float>(settings_->getEffectiveGuiScale(screenHeight_)) : 1.0f;
            float scrollSpeed = 50.0f * guiScale;

            // Invert scroll direction (negative mouseScroll.y to scroll naturally)
            float oldScrollOffset = scrollOffset_;
            scrollOffset_ -= mouseScroll.y * scrollSpeed;

            // Clamp scroll offset
            // Calculate total content height and clamp
            float sliderSpacing = 100.0f * guiScale;
            float keybindSpacing = 45.0f * guiScale;
            float totalContentHeight = screenHeight_ * 0.35f + sliderSpacing * 7.2f + keybindSpacing * 6.5f + 60.0f * guiScale + 50.0f;
            float maxScroll = std::max(0.0f, totalContentHeight - screenHeight_);

            scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maxScroll);

            // Only rebuild UI if scroll offset actually changed
            if (std::abs(scrollOffset_ - oldScrollOffset) > 0.1f) {
                setupUI();
            }
        }

        // Get mouse input
        auto mousePos = InputSystem::getMousePosition();
        bool mouseDown = InputSystem::isMouseButtonPressed(MouseButton::Left);
        bool mouseReleased = mouseWasDown_ && !mouseDown;
        mouseWasDown_ = mouseDown;

        glm::vec2 screenMousePos(mousePos.x, mousePos.y);

        // Update sliders
        for (auto& slider : sliders_) {
            slider->update(screenMousePos, mouseDown, mouseReleased);
        }

        // Update cycling buttons
        bool mouseClicked = InputSystem::isMouseButtonDown(MouseButton::Left);
        for (auto& cyclingButton : cyclingButtons_) {
            cyclingButton->update(screenMousePos, mouseClicked);
        }

        // Update buttons (keybind buttons don't set lastAction_, they set listeningForKeybind_)
        for (size_t i = 0; i < buttons_.size(); ++i) {
            buttons_[i]->update(screenMousePos, mouseClicked);
        }

        // Handle gamepad navigation for buttons (if needed in future)
        if (InputSystem::isGamepadConnected(0)) {
            if (InputSystem::isGamepadButtonDown(GamepadButton::B, 0) ||
                InputSystem::isGamepadButtonDown(GamepadButton::Start, 0)) {
                lastAction_ = Action::Back;
            }
        }

        // ESC key to go back
        if (InputSystem::isKeyDown(KeyCode::Escape)) {
            lastAction_ = Action::Back;
        }

        return lastAction_;
    }

    // Generate vertices for rendering
    std::vector<TextVertex> generateTextVertices(TextRenderer& textRenderer) const {
        std::vector<TextVertex> allVertices;

        // Calculate effective GUI scale
        float guiScale = settings_ ? static_cast<float>(settings_->getEffectiveGuiScale(screenHeight_)) : 1.0f;

        // Add title
        auto titleText = Text::literal("OPTIONS", Style::yellow().withBold(true));
        float titleScale = 4.0f * guiScale;
        float titleWidth = textRenderer.calculateTextWidth(titleText, titleScale);
        float titleX = (screenWidth_ - titleWidth) * 0.5f;
        float titleY = 80.0f;

        auto titleVertices = textRenderer.generateVertices(
            titleText,
            glm::vec2(titleX, titleY),
            titleScale,
            screenWidth_,
            screenHeight_
        );
        allVertices.insert(allVertices.end(), titleVertices.begin(), titleVertices.end());

        // Add slider text
        for (const auto& slider : sliders_) {
            auto sliderVertices = slider->generateTextVertices(
                textRenderer,
                screenWidth_,
                screenHeight_,
                guiScale
            );
            allVertices.insert(allVertices.end(), sliderVertices.begin(), sliderVertices.end());
        }

        // Add cycling button text
        for (const auto& cyclingButton : cyclingButtons_) {
            auto cyclingButtonVertices = cyclingButton->generateTextVertices(
                textRenderer,
                screenWidth_,
                screenHeight_,
                guiScale
            );
            allVertices.insert(allVertices.end(), cyclingButtonVertices.begin(), cyclingButtonVertices.end());
        }

        // Add button text
        for (const auto& button : buttons_) {
            auto buttonVertices = button->generateTextVertices(
                textRenderer,
                screenWidth_,
                screenHeight_,
                guiScale
            );
            allVertices.insert(allVertices.end(), buttonVertices.begin(), buttonVertices.end());
        }

        return allVertices;
    }

    // Generate overlay panel for blur effect
    std::vector<PanelVertex> generateOverlayPanel() const {
        // Blur-like frosted glass overlay
        auto overlay = Panel::createBlurOverlay(screenWidth_, screenHeight_);
        return overlay.generateVertices(screenWidth_, screenHeight_);
    }

    // Generate panel vertices for sliders
    std::vector<PanelVertex> generatePanelVertices(uint32_t screenWidth, uint32_t screenHeight) const {
        std::vector<PanelVertex> allVertices;

        for (const auto& slider : sliders_) {
            auto sliderVertices = slider->generatePanelVertices(screenWidth, screenHeight);
            allVertices.insert(allVertices.end(), sliderVertices.begin(), sliderVertices.end());
        }

        return allVertices;
    }

    // Handle screen resize
    void onResize(uint32_t newWidth, uint32_t newHeight) {
        screenWidth_ = newWidth;
        screenHeight_ = newHeight;
        setupUI();
    }

    // Reset menu state
    void reset() {
        selectedButtonIndex_ = 0;
        lastAction_ = Action::None;

        // Reset sliders to settings values
        if (settings_ && sliders_.size() > 0) {
            sliders_[0]->setValue(settings_->fov);
        }
        if (settings_ && sliders_.size() > 1) {
            sliders_[1]->setValue(static_cast<float>(settings_->renderDistance));
        }
    }

private:
    void setupUI() {
        sliders_.clear();
        cyclingButtons_.clear();
        buttons_.clear();

        // Calculate effective GUI scale for sizing UI elements
        float guiScale = settings_ ? static_cast<float>(settings_->getEffectiveGuiScale(screenHeight_)) : 1.0f;

        // Slider dimensions (scaled)
        float sliderWidth = 400.0f * guiScale;
        float sliderSpacing = 100.0f * guiScale;

        // Center sliders on screen
        float startX = (screenWidth_ - sliderWidth) * 0.5f;
        float startY = screenHeight_ * 0.35f - scrollOffset_;

        // FOV Slider (45 - 120 degrees)
        auto fovSlider = std::make_unique<Slider>(
            "Field of View",
            glm::vec2(startX, startY),
            sliderWidth,
            45.0f, 120.0f,
            settings_ ? settings_->fov : 70.0f,
            true, // Integer values
            guiScale // Scale parameter
        );
        fovSlider->setOnChange([this](float value) {
            if (camera_) {
                camera_->setFov(value);
            }
            if (settings_) {
                settings_->fov = value;
                settings_->save();
            }
        });
        sliders_.push_back(std::move(fovSlider));

        // Render Distance Slider (2 - 32 chunks)
        auto renderDistSlider = std::make_unique<Slider>(
            "Render Distance",
            glm::vec2(startX, startY + sliderSpacing),
            sliderWidth,
            2.0f, 32.0f,
            settings_ ? static_cast<float>(settings_->renderDistance) : 8.0f,
            true, // Integer values
            guiScale // Scale parameter
        );
        renderDistSlider->setOnChange([this](float value) {
            if (chunkManager_) {
                chunkManager_->setRenderDistance(static_cast<int32_t>(value));
            }
            if (settings_) {
                settings_->renderDistance = static_cast<int32_t>(value);
                settings_->save();
            }
        });
        sliders_.push_back(std::move(renderDistSlider));

        // Menu Blur Amount Slider (0 - 10)
        auto blurSlider = std::make_unique<Slider>(
            "Menu Blur",
            glm::vec2(startX, startY + sliderSpacing * 2),
            sliderWidth,
            0.0f, 10.0f,
            settings_ ? static_cast<float>(settings_->menuBlurAmount) : 5.0f,
            true, // Integer values
            guiScale // Scale parameter
        );
        blurSlider->setOnChange([this](float value) {
            if (settings_) {
                settings_->menuBlurAmount = static_cast<int32_t>(value);
                settings_->save();
            }
        });
        sliders_.push_back(std::move(blurSlider));

        // GUI Scale Slider (0-6: 0 = Auto, 1-6 = Manual)
        auto guiScaleSlider = std::make_unique<Slider>(
            "GUI Scale",
            glm::vec2(startX, startY + sliderSpacing * 3),
            sliderWidth,
            0.0f, 6.0f,
            settings_ ? static_cast<float>(settings_->guiScale) : 0.0f,
            true, // Integer values
            guiScale // Scale parameter
        );
        guiScaleSlider->setOnChange([this](float value) {
            if (settings_) {
                settings_->guiScale = static_cast<int32_t>(value);
                settings_->save();
                // Rebuild UI to apply new scale
                setupUI();
            }
        });
        // Custom formatter to display "Auto" for 0
        guiScaleSlider->setValueFormatter([](float value) -> std::string {
            int intValue = static_cast<int>(std::round(value));
            if (intValue == 0) {
                return "Auto";
            }
            return std::to_string(intValue);
        });
        sliders_.push_back(std::move(guiScaleSlider));

        // Mouse Sensitivity Slider (1% - 100%)
        // Internal: 0.01 - 1.0, Display: 1 - 100
        float currentSensitivity = settings_ ? settings_->mouseSensitivity : 0.1f;
        int currentSensitivityPercent = static_cast<int>(currentSensitivity / 0.01f);

        auto mouseSensSlider = std::make_unique<Slider>(
            "Mouse Sensitivity",
            glm::vec2(startX, startY + sliderSpacing * 4),
            sliderWidth,
            1.0f, 100.0f,
            static_cast<float>(currentSensitivityPercent),
            true, // Integer values
            guiScale // Scale parameter
        );
        mouseSensSlider->setOnChange([this](float value) {
            if (camera_) {
                float sensitivity = value * 0.01f; // Convert 1-100 to 0.01-1.0
                camera_->setMouseSensitivity(sensitivity);
            }
            if (settings_) {
                settings_->mouseSensitivity = value * 0.01f;
                settings_->save();
            }
        });
        mouseSensSlider->setValueFormatter([](float value) -> std::string {
            return std::to_string(static_cast<int>(std::round(value))) + "%";
        });
        sliders_.push_back(std::move(mouseSensSlider));

        // Master Volume Slider (0% - 100%)
        // Internal: 0.0 - 1.0, Display: 0 - 100
        float currentVolume = settings_ ? settings_->masterVolume : 0.5f;
        int currentVolumePercent = static_cast<int>(currentVolume * 100.0f);

        auto volumeSlider = std::make_unique<Slider>(
            "Master Volume",
            glm::vec2(startX, startY + sliderSpacing * 5),
            sliderWidth,
            0.0f, 100.0f,
            static_cast<float>(currentVolumePercent),
            true, // Integer values
            guiScale // Scale parameter
        );
        volumeSlider->setOnChange([this](float value) {
            float volume = value * 0.01f; // Convert 0-100 to 0.0-1.0
            if (audioManager_) {
                audioManager_->setMasterVolume(volume);
            }
            if (settings_) {
                settings_->masterVolume = volume;
                settings_->save();
            }
        });
        volumeSlider->setValueFormatter([](float value) -> std::string {
            return std::to_string(static_cast<int>(std::round(value))) + "%";
        });
        sliders_.push_back(std::move(volumeSlider));

        // Audio Device Cycling Button
        if (audioManager_) {
            auto availableDevices = audioManager_->getAvailableDevices();
            std::string currentDevice = settings_ ? settings_->soundDevice.getValue() : "";

            // Find current device index
            size_t currentIndex = 0;
            if (currentDevice.empty() || currentDevice == "Default") {
                currentIndex = 0;
            } else {
                for (size_t i = 0; i < availableDevices.size(); i++) {
                    if (availableDevices[i] == currentDevice) {
                        currentIndex = i;
                        break;
                    }
                }
            }

            auto audioDeviceButton = std::make_unique<CyclingButton>(
                "Audio Device",
                glm::vec2(startX, startY + sliderSpacing * 6),
                sliderWidth,
                availableDevices,
                currentIndex,
                guiScale
            );
            audioDeviceButton->setOnChange([this](const std::string& deviceName) {
                if (audioManager_) {
                    audioManager_->switchDevice(deviceName);
                }
                if (settings_) {
                    settings_->soundDevice = deviceName;
                    settings_->save();
                }
            });
            cyclingButtons_.push_back(std::move(audioDeviceButton));
        }

        // Mipmap Levels Slider (0 - 4)
        // 0 = OFF (no mipmaps), 1-4 = mipmap levels (Minecraft-style)
        auto mipmapSlider = std::make_unique<Slider>(
            "Mipmap Levels",
            glm::vec2(startX, startY + sliderSpacing * 7),
            sliderWidth,
            0.0f, 4.0f,
            settings_ ? static_cast<float>(settings_->mipmapLevels) : 4.0f,
            true, // Integer values
            guiScale // Scale parameter
        );
        mipmapSlider->setOnChange([this](float value) {
            if (settings_) {
                int32_t newValue = static_cast<int32_t>(value);
                if (newValue != settings_->mipmapLevels) {
                    settings_->mipmapLevels = newValue;
                    settings_->save();
                    texturesNeedReload_ = true;  // Flag that textures need reloading
                    spdlog::info("Mipmap level changed to {}, textures will be reloaded", newValue);
                }
            }
        });
        // Custom formatter to display quality description
        mipmapSlider->setValueFormatter([](float value) -> std::string {
            int intValue = static_cast<int>(std::round(value));
            switch (intValue) {
                case 0: return "OFF";
                case 1: return "1";
                case 2: return "2";
                case 3: return "3";
                case 4: return "4";
                default: return std::to_string(intValue);
            }
        });
        sliders_.push_back(std::move(mipmapSlider));

        // Keybind buttons section (scaled)
        float keybindStartY = startY + sliderSpacing * 8.2f;
        float keybindButtonWidth = 250.0f * guiScale;
        float keybindButtonHeight = 35.0f * guiScale;
        float keybindSpacing = 45.0f * guiScale;
        float keybindX = (screenWidth_ - keybindButtonWidth) * 0.5f;

        // Add keybind buttons for movement
        addKeybindButton(KeybindAction::Forward,
                        glm::vec2(keybindX, keybindStartY),
                        glm::vec2(keybindButtonWidth, keybindButtonHeight));

        addKeybindButton(KeybindAction::Back,
                        glm::vec2(keybindX, keybindStartY + keybindSpacing),
                        glm::vec2(keybindButtonWidth, keybindButtonHeight));

        addKeybindButton(KeybindAction::Left,
                        glm::vec2(keybindX, keybindStartY + keybindSpacing * 2),
                        glm::vec2(keybindButtonWidth, keybindButtonHeight));

        addKeybindButton(KeybindAction::Right,
                        glm::vec2(keybindX, keybindStartY + keybindSpacing * 3),
                        glm::vec2(keybindButtonWidth, keybindButtonHeight));

        addKeybindButton(KeybindAction::Jump,
                        glm::vec2(keybindX, keybindStartY + keybindSpacing * 4),
                        glm::vec2(keybindButtonWidth, keybindButtonHeight));

        addKeybindButton(KeybindAction::Sneak,
                        glm::vec2(keybindX, keybindStartY + keybindSpacing * 5),
                        glm::vec2(keybindButtonWidth, keybindButtonHeight));

        // Back button (scaled)
        float buttonWidth = 300.0f * guiScale;
        float buttonHeight = 60.0f * guiScale;
        float buttonX = (screenWidth_ - buttonWidth) * 0.5f;
        float buttonY = keybindStartY + keybindSpacing * 6.5f;

        auto backButton = std::make_unique<Button>(
            "Back",
            glm::vec2(buttonX, buttonY),
            glm::vec2(buttonWidth, buttonHeight)
        );
        backButton->setOnClick([this]() { lastAction_ = Action::Back; });
        buttons_.push_back(std::move(backButton));

        selectedButtonIndex_ = 0;
    }

    void addKeybindButton(KeybindAction action, const glm::vec2& position, const glm::vec2& size) {
        // Get display label from enum using magic_enum
        std::string label = std::string(magic_enum::enum_name(action));

        // Get current keybind
        std::string actionKey = keybindActionToString(action);
        std::string currentKey = "Unbound";

        if (settings_) {
            auto it = settings_->keybinds.find(actionKey);
            if (it != settings_->keybinds.end()) {
                // Extract just the key name (e.g., "key.keyboard.w" -> "W")
                std::string fullKey = it->second;
                size_t lastDot = fullKey.find_last_of('.');
                if (lastDot != std::string::npos) {
                    currentKey = fullKey.substr(lastDot + 1);
                    // Capitalize
                    if (!currentKey.empty()) {
                        currentKey[0] = std::toupper(currentKey[0]);
                    }
                    // Handle multi-part keys (e.g., "left.shift" -> "Left Shift")
                    size_t secondLastDot = fullKey.find_last_of('.', lastDot - 1);
                    if (secondLastDot != std::string::npos) {
                        std::string firstPart = fullKey.substr(secondLastDot + 1, lastDot - secondLastDot - 1);
                        firstPart[0] = std::toupper(firstPart[0]);
                        currentKey = firstPart + " " + currentKey;
                    }
                }
            }
        }

        auto button = std::make_unique<Button>(
            label + ": " + currentKey,
            position,
            size
        );

        button->setOnClick([this, action, label]() {
            // Start listening for key press to rebind
            listeningForKeybind_ = action;
            // Initialize modifier state tracking to current state to avoid immediate triggers
            lastModifierState_[0] = InputSystem::isKeyPressed(KeyCode::LeftShift);
            lastModifierState_[1] = InputSystem::isKeyPressed(KeyCode::RightShift);
            lastModifierState_[2] = InputSystem::isKeyPressed(KeyCode::LeftControl);
            lastModifierState_[3] = InputSystem::isKeyPressed(KeyCode::RightControl);
            lastModifierState_[4] = InputSystem::isKeyPressed(KeyCode::LeftAlt);
            lastModifierState_[5] = InputSystem::isKeyPressed(KeyCode::RightAlt);
            spdlog::info("Press a key to rebind {}", label);
        });

        buttons_.push_back(std::move(button));
    }

    Action getActionForButton(size_t index) const {
        switch (index) {
            case 0: return Action::Back;
            default: return Action::None;
        }
    }

    uint32_t screenWidth_;
    uint32_t screenHeight_;
    Camera* camera_;
    ChunkManager* chunkManager_;
    Settings* settings_;
    AudioManager* audioManager_;
    size_t selectedButtonIndex_;
    Action lastAction_;
    bool mouseWasDown_;
    std::optional<KeybindAction> listeningForKeybind_;  // When set, waiting for key press to rebind
    bool lastModifierState_[6] = {false, false, false, false, false, false};  // Track modifier key states for edge detection
    float scrollOffset_;  // Vertical scroll offset for the menu
    bool texturesNeedReload_;  // Flag set when mipmap level changes

    std::vector<std::unique_ptr<Slider>> sliders_;
    std::vector<std::unique_ptr<CyclingButton>> cyclingButtons_;
    std::vector<std::unique_ptr<Button>> buttons_;
};

} // namespace FarHorizon