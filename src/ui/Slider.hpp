#pragma once

#include "Panel.hpp"
#include "../text/Text.hpp"
#include "../text/TextRenderer.hpp"
#include "../text/Style.hpp"
#include <glm/glm.hpp>
#include <functional>
#include <string>
#include <sstream>
#include <iomanip>

namespace VoxelEngine {

/**
 * Interactive slider UI element for adjusting numeric values.
 * Supports mouse dragging and displays the current value.
 */
class Slider {
public:
    Slider(const std::string& label, const glm::vec2& position, float width,
           float minValue, float maxValue, float currentValue, bool isInteger = false)
        : m_label(label)
        , m_position(position)
        , m_width(width)
        , m_minValue(minValue)
        , m_maxValue(maxValue)
        , m_value(currentValue)
        , m_isInteger(isInteger)
        , m_dragging(false)
        , m_hovered(false) {

        // Slider bar dimensions
        m_barHeight = 8.0f;
        m_knobWidth = 12.0f;
        m_knobHeight = 24.0f;
        m_totalHeight = 80.0f; // Space for label, slider, and value

        updateKnobPosition();
    }

    // Update slider state with mouse input
    bool update(const glm::vec2& mousePos, bool mouseDown, bool mouseReleased) {
        bool valueChanged = false;

        // Get bar position
        glm::vec2 barPos = getBarPosition();

        // Check if mouse is over knob or bar
        glm::vec2 knobPos = getKnobPosition();
        bool mouseOverKnob = isPointInRect(mousePos, knobPos, glm::vec2(m_knobWidth, m_knobHeight));
        bool mouseOverBar = isPointInRect(mousePos, barPos, glm::vec2(m_width, m_barHeight));

        m_hovered = mouseOverKnob || mouseOverBar;

        // Start dragging
        if ((mouseOverKnob || mouseOverBar) && mouseDown && !m_dragging) {
            m_dragging = true;
            // Jump to clicked position if clicked on bar
            if (mouseOverBar && !mouseOverKnob) {
                updateValueFromMouseX(mousePos.x);
                valueChanged = true;
            }
        }

        // Stop dragging
        if (mouseReleased) {
            m_dragging = false;
        }

        // Update value while dragging
        if (m_dragging && mouseDown) {
            float oldValue = m_value;
            updateValueFromMouseX(mousePos.x);
            if (m_value != oldValue) {
                valueChanged = true;
            }
        }

        // Trigger callback if value changed
        if (valueChanged && m_onChangeCallback) {
            m_onChangeCallback(m_value);
        }

        return valueChanged;
    }

    // Generate text vertices for rendering
    std::vector<TextVertex> generateTextVertices(
        TextRenderer& textRenderer,
        uint32_t screenWidth,
        uint32_t screenHeight
    ) const {
        std::vector<TextVertex> allVertices;

        // Label text
        Style labelStyle = Style::white();
        auto labelText = Text::literal(m_label, labelStyle);
        float labelScale = 2.0f;

        auto labelVertices = textRenderer.generateVertices(
            labelText,
            m_position,
            labelScale,
            screenWidth,
            screenHeight
        );
        allVertices.insert(allVertices.end(), labelVertices.begin(), labelVertices.end());

        // Value text
        Style valueStyle = m_hovered || m_dragging ? Style::yellow() : Style::gray();
        std::string valueStr = formatValue(m_value);
        auto valueText = Text::literal(valueStr, valueStyle);
        float valueScale = 2.0f;

        float valueWidth = textRenderer.calculateTextWidth(valueText, valueScale);
        glm::vec2 valuePos = m_position + glm::vec2(m_width - valueWidth, 0.0f);

        auto valueVertices = textRenderer.generateVertices(
            valueText,
            valuePos,
            valueScale,
            screenWidth,
            screenHeight
        );
        allVertices.insert(allVertices.end(), valueVertices.begin(), valueVertices.end());

        return allVertices;
    }

    // Generate panel vertices for rendering slider bar and knob
    std::vector<PanelVertex> generatePanelVertices(uint32_t screenWidth, uint32_t screenHeight) const {
        std::vector<PanelVertex> allVertices;

        // Slider bar background (darker)
        glm::vec2 barPos = getBarPosition();
        glm::vec4 barColor = glm::vec4(0.2f, 0.2f, 0.2f, 0.8f);
        Panel bar(barPos, glm::vec2(m_width, m_barHeight), barColor);
        auto barVertices = bar.generateVertices(screenWidth, screenHeight);
        allVertices.insert(allVertices.end(), barVertices.begin(), barVertices.end());

        // Filled portion of bar (colored)
        float fillWidth = (m_value - m_minValue) / (m_maxValue - m_minValue) * m_width;
        glm::vec4 fillColor = m_dragging ? glm::vec4(1.0f, 0.9f, 0.0f, 0.8f) : glm::vec4(0.6f, 0.6f, 0.6f, 0.8f);
        Panel fill(barPos, glm::vec2(fillWidth, m_barHeight), fillColor);
        auto fillVertices = fill.generateVertices(screenWidth, screenHeight);
        allVertices.insert(allVertices.end(), fillVertices.begin(), fillVertices.end());

        // Slider knob
        glm::vec2 knobPos = getKnobPosition();
        glm::vec4 knobColor = m_hovered || m_dragging ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
        Panel knob(knobPos, glm::vec2(m_knobWidth, m_knobHeight), knobColor);
        auto knobVertices = knob.generateVertices(screenWidth, screenHeight);
        allVertices.insert(allVertices.end(), knobVertices.begin(), knobVertices.end());

        return allVertices;
    }

    // Set callback function for value changes
    void setOnChange(std::function<void(float)> callback) {
        m_onChangeCallback = callback;
    }

    // Getters/Setters
    void setValue(float value) {
        m_value = std::clamp(value, m_minValue, m_maxValue);
        if (m_isInteger) {
            m_value = std::round(m_value);
        }
        updateKnobPosition();
    }

    float getValue() const { return m_value; }
    const glm::vec2& getPosition() const { return m_position; }
    float getTotalHeight() const { return m_totalHeight; }
    bool isDragging() const { return m_dragging; }

private:
    void updateKnobPosition() {
        // Knob position is calculated based on current value
        float t = (m_value - m_minValue) / (m_maxValue - m_minValue);
        m_knobX = t * m_width;
    }

    void updateValueFromMouseX(float mouseX) {
        glm::vec2 barPos = getBarPosition();
        float relativeX = mouseX - barPos.x;
        float t = std::clamp(relativeX / m_width, 0.0f, 1.0f);

        m_value = m_minValue + t * (m_maxValue - m_minValue);
        if (m_isInteger) {
            m_value = std::round(m_value);
        }

        updateKnobPosition();
    }

    glm::vec2 getBarPosition() const {
        // Bar is positioned below the label
        return m_position + glm::vec2(0.0f, 30.0f);
    }

    glm::vec2 getKnobPosition() const {
        glm::vec2 barPos = getBarPosition();
        // Center knob on its position
        return barPos + glm::vec2(m_knobX - m_knobWidth * 0.5f, -m_knobHeight * 0.5f + m_barHeight * 0.5f);
    }

    bool isPointInRect(const glm::vec2& point, const glm::vec2& rectPos, const glm::vec2& rectSize) const {
        return point.x >= rectPos.x &&
               point.x <= rectPos.x + rectSize.x &&
               point.y >= rectPos.y &&
               point.y <= rectPos.y + rectSize.y;
    }

    std::string formatValue(float value) const {
        std::ostringstream oss;
        if (m_isInteger) {
            oss << static_cast<int>(std::round(value));
        } else {
            oss << std::fixed << std::setprecision(1) << value;
        }
        return oss.str();
    }

    std::string m_label;
    glm::vec2 m_position;
    float m_width;
    float m_minValue;
    float m_maxValue;
    float m_value;
    bool m_isInteger;

    // Visual properties
    float m_barHeight;
    float m_knobWidth;
    float m_knobHeight;
    float m_totalHeight;
    float m_knobX;  // Current knob position along the bar

    // State
    bool m_dragging;
    bool m_hovered;

    std::function<void(float)> m_onChangeCallback;
};

} // namespace VoxelEngine