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

namespace FarHorizon {

/**
 * Interactive slider UI element for adjusting numeric values.
 * Supports mouse dragging and displays the current value.
 */
class Slider {
public:
    Slider(const std::string& label, const glm::vec2& position, float width,
           float minValue, float maxValue, float currentValue, bool isInteger = false, float scale = 1.0f)
        : label_(label)
        , position_(position)
        , width_(width)
        , minValue_(minValue)
        , maxValue_(maxValue)
        , value_(currentValue)
        , isInteger_(isInteger)
        , scale_(scale)
        , dragging_(false)
        , hovered_(false) {

        // Slider bar dimensions (scaled)
        barHeight_ = 8.0f * scale;
        knobWidth_ = 12.0f * scale;
        knobHeight_ = 24.0f * scale;
        totalHeight_ = 80.0f * scale; // Space for label, slider, and value

        updateKnobPosition();
    }

    // Update slider state with mouse input
    bool update(const glm::vec2& mousePos, bool mouseDown, bool mouseReleased) {
        bool valueChanged = false;

        // Get bar position
        glm::vec2 barPos = getBarPosition();

        // Check if mouse is over knob or bar
        glm::vec2 knobPos = getKnobPosition();
        bool mouseOverKnob = isPointInRect(mousePos, knobPos, glm::vec2(knobWidth_, knobHeight_));
        bool mouseOverBar = isPointInRect(mousePos, barPos, glm::vec2(width_, barHeight_));

        hovered_ = mouseOverKnob || mouseOverBar;

        // Start dragging
        if ((mouseOverKnob || mouseOverBar) && mouseDown && !dragging_) {
            dragging_ = true;
            // Jump to clicked position if clicked on bar
            if (mouseOverBar && !mouseOverKnob) {
                updateValueFromMouseX(mousePos.x);
                valueChanged = true;
            }
        }

        // Stop dragging
        if (mouseReleased) {
            dragging_ = false;
        }

        // Update value while dragging
        if (dragging_ && mouseDown) {
            float oldValue = value_;
            updateValueFromMouseX(mousePos.x);
            if (value_ != oldValue) {
                valueChanged = true;
            }
        }

        // Trigger callback if value changed
        if (valueChanged && onChangeCallback_) {
            onChangeCallback_(value_);
        }

        return valueChanged;
    }

    // Generate text vertices for rendering
    std::vector<TextVertex> generateTextVertices(
        TextRenderer& textRenderer,
        uint32_t screenWidth,
        uint32_t screenHeight,
        float guiScale = 1.0f
    ) const {
        std::vector<TextVertex> allVertices;

        // Label text
        Style labelStyle = Style::white();
        auto labelText = Text::literal(label_, labelStyle);
        float labelScale = 2.0f * guiScale;

        auto labelVertices = textRenderer.generateVertices(
            labelText,
            position_,
            labelScale,
            screenWidth,
            screenHeight
        );
        allVertices.insert(allVertices.end(), labelVertices.begin(), labelVertices.end());

        // Value text
        Style valueStyle = hovered_ || dragging_ ? Style::yellow() : Style::gray();
        std::string valueStr = formatValue(value_);
        auto valueText = Text::literal(valueStr, valueStyle);
        float valueScale = 2.0f * guiScale;

        float valueWidth = textRenderer.calculateTextWidth(valueText, valueScale);
        glm::vec2 valuePos = position_ + glm::vec2(width_ - valueWidth, 0.0f);

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
        Panel bar(barPos, glm::vec2(width_, barHeight_), barColor);
        auto barVertices = bar.generateVertices(screenWidth, screenHeight);
        allVertices.insert(allVertices.end(), barVertices.begin(), barVertices.end());

        // Filled portion of bar (colored)
        float fillWidth = (value_ - minValue_) / (maxValue_ - minValue_) * width_;
        glm::vec4 fillColor = dragging_ ? glm::vec4(1.0f, 0.9f, 0.0f, 0.8f) : glm::vec4(0.6f, 0.6f, 0.6f, 0.8f);
        Panel fill(barPos, glm::vec2(fillWidth, barHeight_), fillColor);
        auto fillVertices = fill.generateVertices(screenWidth, screenHeight);
        allVertices.insert(allVertices.end(), fillVertices.begin(), fillVertices.end());

        // Slider knob
        glm::vec2 knobPos = getKnobPosition();
        glm::vec4 knobColor = hovered_ || dragging_ ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
        Panel knob(knobPos, glm::vec2(knobWidth_, knobHeight_), knobColor);
        auto knobVertices = knob.generateVertices(screenWidth, screenHeight);
        allVertices.insert(allVertices.end(), knobVertices.begin(), knobVertices.end());

        return allVertices;
    }

    // Set callback function for value changes
    void setOnChange(std::function<void(float)> callback) {
        onChangeCallback_ = callback;
    }

    // Set custom value formatter (e.g., to display "Auto" instead of "0")
    void setValueFormatter(std::function<std::string(float)> formatter) {
        valueFormatter_ = formatter;
    }

    // Getters/Setters
    void setValue(float value) {
        value_ = std::clamp(value, minValue_, maxValue_);
        if (isInteger_) {
            value_ = std::round(value_);
        }
        updateKnobPosition();
    }

    float getValue() const { return value_; }
    const glm::vec2& getPosition() const { return position_; }
    float getTotalHeight() const { return totalHeight_; }
    bool isDragging() const { return dragging_; }

private:
    void updateKnobPosition() {
        // Knob position is calculated based on current value
        float t = (value_ - minValue_) / (maxValue_ - minValue_);
        knobX_ = t * width_;
    }

    void updateValueFromMouseX(float mouseX) {
        glm::vec2 barPos = getBarPosition();
        float relativeX = mouseX - barPos.x;
        float t = std::clamp(relativeX / width_, 0.0f, 1.0f);

        value_ = minValue_ + t * (maxValue_ - minValue_);
        if (isInteger_) {
            value_ = std::round(value_);
        }

        updateKnobPosition();
    }

    glm::vec2 getBarPosition() const {
        // Bar is positioned below the label (scaled)
        return position_ + glm::vec2(0.0f, 30.0f * scale_);
    }

    glm::vec2 getKnobPosition() const {
        glm::vec2 barPos = getBarPosition();
        // Center knob on its position
        return barPos + glm::vec2(knobX_ - knobWidth_ * 0.5f, -knobHeight_ * 0.5f + barHeight_ * 0.5f);
    }

    bool isPointInRect(const glm::vec2& point, const glm::vec2& rectPos, const glm::vec2& rectSize) const {
        return point.x >= rectPos.x &&
               point.x <= rectPos.x + rectSize.x &&
               point.y >= rectPos.y &&
               point.y <= rectPos.y + rectSize.y;
    }

    std::string formatValue(float value) const {
        // Use custom formatter if provided
        if (valueFormatter_) {
            return valueFormatter_(value);
        }

        // Default formatting
        std::ostringstream oss;
        if (isInteger_) {
            oss << static_cast<int>(std::round(value));
        } else {
            oss << std::fixed << std::setprecision(1) << value;
        }
        return oss.str();
    }

    std::string label_;
    glm::vec2 position_;
    float width_;
    float minValue_;
    float maxValue_;
    float value_;
    bool isInteger_;
    float scale_;

    // Visual properties
    float barHeight_;
    float knobWidth_;
    float knobHeight_;
    float totalHeight_;
    float knobX_;  // Current knob position along the bar

    // State
    bool dragging_;
    bool hovered_;

    std::function<void(float)> onChangeCallback_;
    std::function<std::string(float)> valueFormatter_;
};

} // namespace FarHorizon