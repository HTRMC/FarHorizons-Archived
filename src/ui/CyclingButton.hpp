#pragma once

#include "../text/Text.hpp"
#include "../text/TextRenderer.hpp"
#include "../text/Style.hpp"
#include <glm/glm.hpp>
#include <functional>
#include <string>
#include <vector>

namespace FarHorizon {

/**
 * Cycling button UI element that cycles through a list of options when clicked.
 * Displays "Label: Option" format.
 */
class CyclingButton {
public:
    CyclingButton(const std::string& label, const glm::vec2& position, float width,
                  const std::vector<std::string>& options, size_t currentIndex = 0, float scale = 1.0f)
        : m_label(label)
        , m_position(position)
        , m_width(width)
        , m_options(options)
        , m_currentIndex(currentIndex)
        , m_scale(scale)
        , m_hovered(false)
        , m_selected(false)
        , m_enabled(true) {

        m_height = 60.0f * scale; // Height for the button

        if (!m_options.empty() && m_currentIndex >= m_options.size()) {
            m_currentIndex = 0;
        }
    }

    // Set callback function for when option changes
    void setOnChange(std::function<void(const std::string&)> callback) {
        m_onChangeCallback = callback;
    }

    // Update button state with mouse input
    bool update(const glm::vec2& mousePos, bool mouseClicked) {
        if (!m_enabled) {
            m_hovered = false;
            return false;
        }

        // Update hover state
        m_hovered = isMouseOver(mousePos);

        if (m_hovered && mouseClicked) {
            cycleNext();
            return true;
        }
        return false;
    }

    // Cycle to the next option (wraps around)
    void cycleNext() {
        if (m_options.empty()) return;

        m_currentIndex = (m_currentIndex + 1) % m_options.size();

        if (m_onChangeCallback) {
            m_onChangeCallback(m_options[m_currentIndex]);
        }
    }

    // Activate button (for keyboard navigation)
    void activate() {
        if (m_enabled) {
            cycleNext();
        }
    }

    // Generate text vertices for rendering
    std::vector<TextVertex> generateTextVertices(
        TextRenderer& textRenderer,
        uint32_t screenWidth,
        uint32_t screenHeight,
        float guiScale = 1.0f
    ) const {
        // Choose style based on button state
        Style labelStyle;
        if (!m_enabled) {
            labelStyle = Style::darkGray();
        } else if (m_selected) {
            labelStyle = Style::yellow();
        } else if (m_hovered) {
            labelStyle = Style::yellow();
        } else {
            labelStyle = Style::white();
        }

        // Build the display text: "Label: CurrentOption"
        std::string displayText = m_label + ": ";
        std::string currentOption = m_options.empty() ? "None" : m_options[m_currentIndex];

        auto text = Text::literal(displayText + currentOption, labelStyle);

        // Calculate centered position
        float scale = 2.5f * guiScale;
        float textWidth = textRenderer.calculateTextWidth(text, scale);
        float textHeight = textRenderer.calculateTextHeight(text, scale);

        glm::vec2 textPos = m_position + glm::vec2(
            (m_width - textWidth) * 0.5f,
            (m_height - textHeight) * 0.5f
        );

        return textRenderer.generateVertices(text, textPos, scale, screenWidth, screenHeight);
    }

    // Getters/Setters
    void setPosition(const glm::vec2& pos) { m_position = pos; }
    void setSelected(bool selected) { m_selected = selected; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    void setHovered(bool hovered) { m_hovered = hovered; }

    const glm::vec2& getPosition() const { return m_position; }
    float getHeight() const { return m_height; }
    bool isSelected() const { return m_selected; }
    bool isHovered() const { return m_hovered; }
    bool isEnabled() const { return m_enabled; }

    const std::string& getCurrentOption() const {
        return m_options.empty() ? "" : m_options[m_currentIndex];
    }

    size_t getCurrentIndex() const { return m_currentIndex; }

    void setOptions(const std::vector<std::string>& options, size_t index = 0) {
        m_options = options;
        m_currentIndex = index;
        if (!m_options.empty() && m_currentIndex >= m_options.size()) {
            m_currentIndex = 0;
        }
    }

    void setCurrentIndex(size_t index) {
        if (!m_options.empty() && index < m_options.size()) {
            m_currentIndex = index;
        }
    }

private:
    bool isMouseOver(const glm::vec2& mousePos) const {
        return mousePos.x >= m_position.x &&
               mousePos.x <= m_position.x + m_width &&
               mousePos.y >= m_position.y &&
               mousePos.y <= m_position.y + m_height;
    }

    std::string m_label;
    glm::vec2 m_position;
    float m_width;
    float m_height;
    std::vector<std::string> m_options;
    size_t m_currentIndex;
    float m_scale;
    bool m_hovered;
    bool m_selected;
    bool m_enabled;
    std::function<void(const std::string&)> m_onChangeCallback;
};

} // namespace FarHorizon