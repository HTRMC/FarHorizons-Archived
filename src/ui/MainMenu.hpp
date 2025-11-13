#pragma once

#include "Button.hpp"
#include "Panel.hpp"
#include "../text/TextRenderer.hpp"
#include "../core/InputSystem.hpp"
#include <memory>
#include <vector>

namespace FarHorizon {

/**
 * Main menu UI with Singleplayer, Options, and Quit buttons.
 * Handles keyboard and mouse input for navigation and selection.
 */
class MainMenu {
public:
    enum class Action {
        None,
        Singleplayer,
        OpenOptions,
        Quit
    };

    MainMenu(uint32_t screenWidth, uint32_t screenHeight)
        : screenWidth_(screenWidth)
        , screenHeight_(screenHeight)
        , selectedButtonIndex_(0)
        , lastAction_(Action::None) {
        setupButtons();
    }

    // Update menu state with input
    Action update(float deltaTime) {
        lastAction_ = Action::None;

        // Handle gamepad navigation only if gamepad is connected
        if (InputSystem::isGamepadConnected(0)) {
            if (InputSystem::isGamepadButtonDown(GamepadButton::DpadUp, 0)) {
                selectPreviousButton();
            }
            if (InputSystem::isGamepadButtonDown(GamepadButton::DpadDown, 0)) {
                selectNextButton();
            }
            if (InputSystem::isGamepadButtonDown(GamepadButton::A, 0)) {
                activateSelectedButton();
            }
        }

        // Handle mouse input
        auto mousePos = InputSystem::getMousePosition();
        bool mouseClicked = InputSystem::isMouseButtonDown(MouseButton::Left);

        // Convert mouse position to screen coordinates (GLFW gives us coordinates already in pixels)
        glm::vec2 screenMousePos(mousePos.x, mousePos.y);

        // When no gamepad is connected, clear selection (let hover handle styling)
        bool useSelection = InputSystem::isGamepadConnected(0);

        // Update buttons
        for (size_t i = 0; i < buttons_.size(); ++i) {
            if (buttons_[i]->update(screenMousePos, mouseClicked)) {
                lastAction_ = getActionForButton(i);
            }

            // Update selection based on hover (only if gamepad connected)
            if (buttons_[i]->isHovered() && useSelection) {
                selectedButtonIndex_ = i;
            }
        }

        // Update button selection states (only when using gamepad)
        for (size_t i = 0; i < buttons_.size(); ++i) {
            buttons_[i]->setSelected(useSelection && i == selectedButtonIndex_);
        }

        return lastAction_;
    }

    // Generate vertices for rendering
    std::vector<TextVertex> generateTextVertices(TextRenderer& textRenderer) const {
        std::vector<TextVertex> allVertices;

        // Add title
        auto titleText = Text::literal("VULKAN VOXEL ENGINE", Style::yellow().withBold(true));
        float titleWidth = textRenderer.calculateTextWidth(titleText, 4.0f);
        float titleX = (screenWidth_ - titleWidth) * 0.5f;
        float titleY = 100.0f;

        auto titleVertices = textRenderer.generateVertices(
            titleText,
            glm::vec2(titleX, titleY),
            4.0f,
            screenWidth_,
            screenHeight_
        );
        allVertices.insert(allVertices.end(), titleVertices.begin(), titleVertices.end());

        // Add button text
        for (const auto& button : buttons_) {
            auto buttonVertices = button->generateTextVertices(
                textRenderer,
                screenWidth_,
                screenHeight_
            );
            allVertices.insert(allVertices.end(), buttonVertices.begin(), buttonVertices.end());
        }

        return allVertices;
    }

    // Handle screen resize
    void onResize(uint32_t newWidth, uint32_t newHeight) {
        screenWidth_ = newWidth;
        screenHeight_ = newHeight;
        setupButtons();
    }

    // Reset menu state
    void reset() {
        selectedButtonIndex_ = 0;
        lastAction_ = Action::None;
    }

private:
    void setupButtons() {
        buttons_.clear();

        // Button dimensions
        float buttonWidth = 300.0f;
        float buttonHeight = 60.0f;
        float buttonSpacing = 20.0f;

        // Center buttons on screen
        float startX = (screenWidth_ - buttonWidth) * 0.5f;
        float startY = screenHeight_ * 0.4f;

        // Create buttons
        auto singleplayerButton = std::make_unique<Button>(
            "Singleplayer",
            glm::vec2(startX, startY),
            glm::vec2(buttonWidth, buttonHeight)
        );
        singleplayerButton->setOnClick([this]() { lastAction_ = Action::Singleplayer; });

        auto optionsButton = std::make_unique<Button>(
            "Options",
            glm::vec2(startX, startY + buttonHeight + buttonSpacing),
            glm::vec2(buttonWidth, buttonHeight)
        );
        optionsButton->setOnClick([this]() { lastAction_ = Action::OpenOptions; });

        auto quitButton = std::make_unique<Button>(
            "Quit",
            glm::vec2(startX, startY + (buttonHeight + buttonSpacing) * 2),
            glm::vec2(buttonWidth, buttonHeight)
        );
        quitButton->setOnClick([this]() { lastAction_ = Action::Quit; });

        buttons_.push_back(std::move(singleplayerButton));
        buttons_.push_back(std::move(optionsButton));
        buttons_.push_back(std::move(quitButton));

        // Set initial selection
        buttons_[selectedButtonIndex_]->setSelected(true);
    }

    void selectPreviousButton() {
        if (selectedButtonIndex_ > 0) {
            selectedButtonIndex_--;
        }
    }

    void selectNextButton() {
        if (selectedButtonIndex_ < buttons_.size() - 1) {
            selectedButtonIndex_++;
        }
    }

    void activateSelectedButton() {
        if (selectedButtonIndex_ < buttons_.size()) {
            buttons_[selectedButtonIndex_]->activate();
        }
    }

    Action getActionForButton(size_t index) const {
        switch (index) {
            case 0: return Action::Singleplayer;
            case 1: return Action::OpenOptions;
            case 2: return Action::Quit;
            default: return Action::None;
        }
    }

    uint32_t screenWidth_;
    uint32_t screenHeight_;
    size_t selectedButtonIndex_;
    Action lastAction_;

    std::vector<std::unique_ptr<Button>> buttons_;
};

} // namespace FarHorizon