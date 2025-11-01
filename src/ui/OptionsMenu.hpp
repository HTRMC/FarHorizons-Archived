#pragma once

#include "Button.hpp"
#include "Slider.hpp"
#include "Panel.hpp"
#include "../text/TextRenderer.hpp"
#include "../core/InputSystem.hpp"
#include "../core/Camera.hpp"
#include "../core/Settings.hpp"
#include "../core/KeybindAction.hpp"
#include "../world/ChunkManager.hpp"
#include <memory>
#include <vector>
#include <optional>
#include <magic_enum/magic_enum.hpp>

namespace VoxelEngine {

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

    OptionsMenu(uint32_t screenWidth, uint32_t screenHeight, Camera* camera, ChunkManager* chunkManager, Settings* settings)
        : m_screenWidth(screenWidth)
        , m_screenHeight(screenHeight)
        , m_camera(camera)
        , m_chunkManager(chunkManager)
        , m_settings(settings)
        , m_selectedButtonIndex(0)
        , m_lastAction(Action::None)
        , m_mouseWasDown(false) {
        setupUI();
    }

    // Update menu state with input
    Action update(float deltaTime) {
        m_lastAction = Action::None;

        // If listening for keybind, wait for any key press
        if (m_listeningForKeybind.has_value()) {
            // Special handling for modifier keys (they don't always trigger isKeyDown)
            // Check if any modifiers are currently pressed
            KeyCode detectedModifier = KeyCode::Unknown;
            if (InputSystem::isKeyPressed(KeyCode::LeftShift) && !m_lastModifierState[0]) {
                detectedModifier = KeyCode::LeftShift;
                m_lastModifierState[0] = true;
            } else if (InputSystem::isKeyPressed(KeyCode::RightShift) && !m_lastModifierState[1]) {
                detectedModifier = KeyCode::RightShift;
                m_lastModifierState[1] = true;
            } else if (InputSystem::isKeyPressed(KeyCode::LeftControl) && !m_lastModifierState[2]) {
                detectedModifier = KeyCode::LeftControl;
                m_lastModifierState[2] = true;
            } else if (InputSystem::isKeyPressed(KeyCode::RightControl) && !m_lastModifierState[3]) {
                detectedModifier = KeyCode::RightControl;
                m_lastModifierState[3] = true;
            } else if (InputSystem::isKeyPressed(KeyCode::LeftAlt) && !m_lastModifierState[4]) {
                detectedModifier = KeyCode::LeftAlt;
                m_lastModifierState[4] = true;
            } else if (InputSystem::isKeyPressed(KeyCode::RightAlt) && !m_lastModifierState[5]) {
                detectedModifier = KeyCode::RightAlt;
                m_lastModifierState[5] = true;
            }

            // Reset modifier states when released
            if (!InputSystem::isKeyPressed(KeyCode::LeftShift)) m_lastModifierState[0] = false;
            if (!InputSystem::isKeyPressed(KeyCode::RightShift)) m_lastModifierState[1] = false;
            if (!InputSystem::isKeyPressed(KeyCode::LeftControl)) m_lastModifierState[2] = false;
            if (!InputSystem::isKeyPressed(KeyCode::RightControl)) m_lastModifierState[3] = false;
            if (!InputSystem::isKeyPressed(KeyCode::LeftAlt)) m_lastModifierState[4] = false;
            if (!InputSystem::isKeyPressed(KeyCode::RightAlt)) m_lastModifierState[5] = false;

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
                if (m_settings) {
                    std::string actionKey = keybindActionToString(m_listeningForKeybind.value());
                    m_settings->keybinds[actionKey] = settingsKey;
                    m_settings->save();

                    if (m_camera) {
                        m_camera->setKeybinds(m_settings->keybinds);
                    }

                    spdlog::info("Rebound {} to {}", actionKey, settingsKey);
                }

                setupUI();
                m_listeningForKeybind.reset();
                return m_lastAction;
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
                    if (m_settings) {
                        std::string actionKey = keybindActionToString(m_listeningForKeybind.value());
                        m_settings->keybinds[actionKey] = settingsKey;
                        m_settings->save();

                        // Apply keybinds to camera
                        if (m_camera) {
                            m_camera->setKeybinds(m_settings->keybinds);
                        }

                        spdlog::info("Rebound {} to {}", actionKey, settingsKey);
                    }

                    // Rebuild UI to show new keybind
                    setupUI();
                    m_listeningForKeybind.reset();
                    return m_lastAction;
                }
            }

            // Allow ESC to cancel rebinding
            if (InputSystem::isKeyDown(KeyCode::Escape)) {
                m_listeningForKeybind.reset();
                spdlog::info("Cancelled keybind");
                return m_lastAction;
            }

            return m_lastAction; // Don't process other input while listening
        }

        // Get mouse input
        auto mousePos = InputSystem::getMousePosition();
        bool mouseDown = InputSystem::isMouseButtonPressed(MouseButton::Left);
        bool mouseReleased = m_mouseWasDown && !mouseDown;
        m_mouseWasDown = mouseDown;

        glm::vec2 screenMousePos(mousePos.x, mousePos.y);

        // Update sliders
        for (auto& slider : m_sliders) {
            slider->update(screenMousePos, mouseDown, mouseReleased);
        }

        // Update buttons (keybind buttons don't set m_lastAction, they set m_listeningForKeybind)
        bool mouseClicked = InputSystem::isMouseButtonDown(MouseButton::Left);
        for (size_t i = 0; i < m_buttons.size(); ++i) {
            m_buttons[i]->update(screenMousePos, mouseClicked);
        }

        // Handle gamepad navigation for buttons (if needed in future)
        if (InputSystem::isGamepadConnected(0)) {
            if (InputSystem::isGamepadButtonDown(GamepadButton::B, 0) ||
                InputSystem::isGamepadButtonDown(GamepadButton::Start, 0)) {
                m_lastAction = Action::Back;
            }
        }

        // ESC key to go back
        if (InputSystem::isKeyDown(KeyCode::Escape)) {
            m_lastAction = Action::Back;
        }

        return m_lastAction;
    }

    // Generate vertices for rendering
    std::vector<TextVertex> generateTextVertices(TextRenderer& textRenderer) const {
        std::vector<TextVertex> allVertices;

        // Add title
        auto titleText = Text::literal("OPTIONS", Style::yellow().withBold(true));
        float titleWidth = textRenderer.calculateTextWidth(titleText, 4.0f);
        float titleX = (m_screenWidth - titleWidth) * 0.5f;
        float titleY = 80.0f;

        auto titleVertices = textRenderer.generateVertices(
            titleText,
            glm::vec2(titleX, titleY),
            4.0f,
            m_screenWidth,
            m_screenHeight
        );
        allVertices.insert(allVertices.end(), titleVertices.begin(), titleVertices.end());

        // Add slider text
        for (const auto& slider : m_sliders) {
            auto sliderVertices = slider->generateTextVertices(
                textRenderer,
                m_screenWidth,
                m_screenHeight
            );
            allVertices.insert(allVertices.end(), sliderVertices.begin(), sliderVertices.end());
        }

        // Add button text
        for (const auto& button : m_buttons) {
            auto buttonVertices = button->generateTextVertices(
                textRenderer,
                m_screenWidth,
                m_screenHeight
            );
            allVertices.insert(allVertices.end(), buttonVertices.begin(), buttonVertices.end());
        }

        return allVertices;
    }

    // Generate overlay panel for blur effect
    std::vector<PanelVertex> generateOverlayPanel() const {
        // Blur-like frosted glass overlay
        auto overlay = Panel::createBlurOverlay(m_screenWidth, m_screenHeight);
        return overlay.generateVertices(m_screenWidth, m_screenHeight);
    }

    // Generate panel vertices for sliders
    std::vector<PanelVertex> generatePanelVertices(uint32_t screenWidth, uint32_t screenHeight) const {
        std::vector<PanelVertex> allVertices;

        for (const auto& slider : m_sliders) {
            auto sliderVertices = slider->generatePanelVertices(screenWidth, screenHeight);
            allVertices.insert(allVertices.end(), sliderVertices.begin(), sliderVertices.end());
        }

        return allVertices;
    }

    // Handle screen resize
    void onResize(uint32_t newWidth, uint32_t newHeight) {
        m_screenWidth = newWidth;
        m_screenHeight = newHeight;
        setupUI();
    }

    // Reset menu state
    void reset() {
        m_selectedButtonIndex = 0;
        m_lastAction = Action::None;

        // Reset sliders to settings values
        if (m_settings && m_sliders.size() > 0) {
            m_sliders[0]->setValue(m_settings->fov);
        }
        if (m_settings && m_sliders.size() > 1) {
            m_sliders[1]->setValue(static_cast<float>(m_settings->renderDistance));
        }
    }

private:
    void setupUI() {
        m_sliders.clear();
        m_buttons.clear();

        // Slider dimensions
        float sliderWidth = 400.0f;
        float sliderSpacing = 100.0f;

        // Center sliders on screen
        float startX = (m_screenWidth - sliderWidth) * 0.5f;
        float startY = m_screenHeight * 0.35f;

        // FOV Slider (45 - 120 degrees)
        auto fovSlider = std::make_unique<Slider>(
            "Field of View",
            glm::vec2(startX, startY),
            sliderWidth,
            45.0f, 120.0f,
            m_settings ? m_settings->fov : 70.0f,
            true // Integer values
        );
        fovSlider->setOnChange([this](float value) {
            if (m_camera) {
                m_camera->setFov(value);
            }
            if (m_settings) {
                m_settings->fov = value;
                m_settings->save();
            }
        });
        m_sliders.push_back(std::move(fovSlider));

        // Render Distance Slider (2 - 32 chunks)
        auto renderDistSlider = std::make_unique<Slider>(
            "Render Distance",
            glm::vec2(startX, startY + sliderSpacing),
            sliderWidth,
            2.0f, 32.0f,
            m_settings ? static_cast<float>(m_settings->renderDistance) : 8.0f,
            true // Integer values
        );
        renderDistSlider->setOnChange([this](float value) {
            if (m_chunkManager) {
                m_chunkManager->setRenderDistance(static_cast<int32_t>(value));
            }
            if (m_settings) {
                m_settings->renderDistance = static_cast<int32_t>(value);
                m_settings->save();
            }
        });
        m_sliders.push_back(std::move(renderDistSlider));

        // Menu Blur Amount Slider (0 - 10)
        auto blurSlider = std::make_unique<Slider>(
            "Menu Blur",
            glm::vec2(startX, startY + sliderSpacing * 2),
            sliderWidth,
            0.0f, 10.0f,
            m_settings ? static_cast<float>(m_settings->menuBlurAmount) : 5.0f,
            true // Integer values
        );
        blurSlider->setOnChange([this](float value) {
            if (m_settings) {
                m_settings->menuBlurAmount = static_cast<int32_t>(value);
                m_settings->save();
            }
        });
        m_sliders.push_back(std::move(blurSlider));

        // GUI Scale Slider (0-6: 0 = Auto, 1-6 = Manual)
        auto guiScaleSlider = std::make_unique<Slider>(
            "GUI Scale",
            glm::vec2(startX, startY + sliderSpacing * 3),
            sliderWidth,
            0.0f, 6.0f,
            m_settings ? static_cast<float>(m_settings->guiScale) : 0.0f,
            true // Integer values
        );
        guiScaleSlider->setOnChange([this](float value) {
            if (m_settings) {
                m_settings->guiScale = static_cast<int32_t>(value);
                m_settings->save();
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
        m_sliders.push_back(std::move(guiScaleSlider));

        // Keybind buttons section
        float keybindStartY = startY + sliderSpacing * 4.2f;
        float keybindButtonWidth = 250.0f;
        float keybindButtonHeight = 35.0f;
        float keybindSpacing = 45.0f;
        float keybindX = (m_screenWidth - keybindButtonWidth) * 0.5f;

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

        // Back button
        float buttonWidth = 300.0f;
        float buttonHeight = 60.0f;
        float buttonX = (m_screenWidth - buttonWidth) * 0.5f;
        float buttonY = keybindStartY + keybindSpacing * 6.5f;

        auto backButton = std::make_unique<Button>(
            "Back",
            glm::vec2(buttonX, buttonY),
            glm::vec2(buttonWidth, buttonHeight)
        );
        backButton->setOnClick([this]() { m_lastAction = Action::Back; });
        m_buttons.push_back(std::move(backButton));

        m_selectedButtonIndex = 0;
    }

    void addKeybindButton(KeybindAction action, const glm::vec2& position, const glm::vec2& size) {
        // Get display label from enum using magic_enum
        std::string label = std::string(magic_enum::enum_name(action));

        // Get current keybind
        std::string actionKey = keybindActionToString(action);
        std::string currentKey = "Unbound";

        if (m_settings) {
            auto it = m_settings->keybinds.find(actionKey);
            if (it != m_settings->keybinds.end()) {
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
            m_listeningForKeybind = action;
            // Initialize modifier state tracking to current state to avoid immediate triggers
            m_lastModifierState[0] = InputSystem::isKeyPressed(KeyCode::LeftShift);
            m_lastModifierState[1] = InputSystem::isKeyPressed(KeyCode::RightShift);
            m_lastModifierState[2] = InputSystem::isKeyPressed(KeyCode::LeftControl);
            m_lastModifierState[3] = InputSystem::isKeyPressed(KeyCode::RightControl);
            m_lastModifierState[4] = InputSystem::isKeyPressed(KeyCode::LeftAlt);
            m_lastModifierState[5] = InputSystem::isKeyPressed(KeyCode::RightAlt);
            spdlog::info("Press a key to rebind {}", label);
        });

        m_buttons.push_back(std::move(button));
    }

    Action getActionForButton(size_t index) const {
        switch (index) {
            case 0: return Action::Back;
            default: return Action::None;
        }
    }

    uint32_t m_screenWidth;
    uint32_t m_screenHeight;
    Camera* m_camera;
    ChunkManager* m_chunkManager;
    Settings* m_settings;
    size_t m_selectedButtonIndex;
    Action m_lastAction;
    bool m_mouseWasDown;
    std::optional<KeybindAction> m_listeningForKeybind;  // When set, waiting for key press to rebind
    bool m_lastModifierState[6] = {false, false, false, false, false, false};  // Track modifier key states for edge detection

    std::vector<std::unique_ptr<Slider>> m_sliders;
    std::vector<std::unique_ptr<Button>> m_buttons;
};

} // namespace VoxelEngine