#pragma once

#include "Button.hpp"
#include "Panel.hpp"
#include "../text/TextRenderer.hpp"
#include "../core/InputSystem.hpp"
#include <memory>
#include <vector>

namespace VoxelEngine {

/**
 * Pause menu UI with Resume, Options, and Quit buttons.
 * Handles keyboard and mouse input for navigation and selection.
 */
class PauseMenu {
public:
    enum class Action {
        None,
        Resume,
        OpenOptions,
        Quit
    };

    PauseMenu(uint32_t screenWidth, uint32_t screenHeight)
        : m_screenWidth(screenWidth)
        , m_screenHeight(screenHeight)
        , m_selectedButtonIndex(0)
        , m_lastAction(Action::None) {
        setupButtons();
    }

    // Update menu state with input
    Action update(float deltaTime) {
        m_lastAction = Action::None;

        // Handle ESC key to resume (unpause)
        if (InputSystem::isKeyDown(KeyCode::Escape)) {
            m_lastAction = Action::Resume;
            return m_lastAction;
        }

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
        for (size_t i = 0; i < m_buttons.size(); ++i) {
            if (m_buttons[i]->update(screenMousePos, mouseClicked)) {
                m_lastAction = getActionForButton(i);
            }

            // Update selection based on hover (only if gamepad connected)
            if (m_buttons[i]->isHovered() && useSelection) {
                m_selectedButtonIndex = i;
            }
        }

        // Update button selection states (only when using gamepad)
        for (size_t i = 0; i < m_buttons.size(); ++i) {
            m_buttons[i]->setSelected(useSelection && i == m_selectedButtonIndex);
        }

        return m_lastAction;
    }

    // Generate vertices for rendering
    std::vector<TextVertex> generateTextVertices(TextRenderer& textRenderer) const {
        std::vector<TextVertex> allVertices;

        // Add title
        auto titleText = Text::literal("PAUSED", Style::yellow().withBold(true));
        float titleWidth = textRenderer.calculateTextWidth(titleText, 5.0f);
        float titleX = (m_screenWidth - titleWidth) * 0.5f;
        float titleY = 150.0f;

        auto titleVertices = textRenderer.generateVertices(
            titleText,
            glm::vec2(titleX, titleY),
            5.0f,
            m_screenWidth,
            m_screenHeight
        );
        allVertices.insert(allVertices.end(), titleVertices.begin(), titleVertices.end());

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

    // Handle screen resize
    void onResize(uint32_t newWidth, uint32_t newHeight) {
        m_screenWidth = newWidth;
        m_screenHeight = newHeight;
        setupButtons();
    }

    // Reset menu state
    void reset() {
        m_selectedButtonIndex = 0;
        m_lastAction = Action::None;
    }

private:
    void setupButtons() {
        m_buttons.clear();

        // Button dimensions
        float buttonWidth = 300.0f;
        float buttonHeight = 60.0f;
        float buttonSpacing = 20.0f;

        // Center buttons on screen
        float startX = (m_screenWidth - buttonWidth) * 0.5f;
        float startY = m_screenHeight * 0.4f;

        // Create buttons
        auto resumeButton = std::make_unique<Button>(
            "Resume",
            glm::vec2(startX, startY),
            glm::vec2(buttonWidth, buttonHeight)
        );
        resumeButton->setOnClick([this]() { m_lastAction = Action::Resume; });

        auto optionsButton = std::make_unique<Button>(
            "Options",
            glm::vec2(startX, startY + buttonHeight + buttonSpacing),
            glm::vec2(buttonWidth, buttonHeight)
        );
        optionsButton->setOnClick([this]() { m_lastAction = Action::OpenOptions; });

        auto quitButton = std::make_unique<Button>(
            "Quit",
            glm::vec2(startX, startY + (buttonHeight + buttonSpacing) * 2),
            glm::vec2(buttonWidth, buttonHeight)
        );
        quitButton->setOnClick([this]() { m_lastAction = Action::Quit; });

        m_buttons.push_back(std::move(resumeButton));
        m_buttons.push_back(std::move(optionsButton));
        m_buttons.push_back(std::move(quitButton));

        // Set initial selection
        m_buttons[m_selectedButtonIndex]->setSelected(true);
    }

    void selectPreviousButton() {
        if (m_selectedButtonIndex > 0) {
            m_selectedButtonIndex--;
        }
    }

    void selectNextButton() {
        if (m_selectedButtonIndex < m_buttons.size() - 1) {
            m_selectedButtonIndex++;
        }
    }

    void activateSelectedButton() {
        if (m_selectedButtonIndex < m_buttons.size()) {
            m_buttons[m_selectedButtonIndex]->activate();
        }
    }

    Action getActionForButton(size_t index) const {
        switch (index) {
            case 0: return Action::Resume;
            case 1: return Action::OpenOptions;
            case 2: return Action::Quit;
            default: return Action::None;
        }
    }

    uint32_t m_screenWidth;
    uint32_t m_screenHeight;
    size_t m_selectedButtonIndex;
    Action m_lastAction;

    std::vector<std::unique_ptr<Button>> m_buttons;
};

} // namespace VoxelEngine