#pragma once

#include "Button.hpp"
#include "Slider.hpp"
#include "Panel.hpp"
#include "../text/TextRenderer.hpp"
#include "../core/InputSystem.hpp"
#include "../core/Camera.hpp"
#include "../core/Settings.hpp"
#include "../world/ChunkManager.hpp"
#include <memory>
#include <vector>

namespace VoxelEngine {

/**
 * Options menu UI with FOV and render distance sliders.
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

        // Update buttons
        bool mouseClicked = InputSystem::isMouseButtonDown(MouseButton::Left);
        for (size_t i = 0; i < m_buttons.size(); ++i) {
            if (m_buttons[i]->update(screenMousePos, mouseClicked)) {
                m_lastAction = getActionForButton(i);
            }
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

        // Back button
        float buttonWidth = 300.0f;
        float buttonHeight = 60.0f;
        float buttonX = (m_screenWidth - buttonWidth) * 0.5f;
        float buttonY = startY + sliderSpacing * 2.5f;

        auto backButton = std::make_unique<Button>(
            "Back",
            glm::vec2(buttonX, buttonY),
            glm::vec2(buttonWidth, buttonHeight)
        );
        backButton->setOnClick([this]() { m_lastAction = Action::Back; });
        m_buttons.push_back(std::move(backButton));

        m_selectedButtonIndex = 0;
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

    std::vector<std::unique_ptr<Slider>> m_sliders;
    std::vector<std::unique_ptr<Button>> m_buttons;
};

} // namespace VoxelEngine