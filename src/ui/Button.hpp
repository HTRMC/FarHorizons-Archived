#pragma once

#include "../text/Text.hpp"
#include "../text/TextRenderer.hpp"
#include "../text/Style.hpp"
#include <glm/glm.hpp>
#include <functional>
#include <string>

namespace VoxelEngine {

/**
 * Interactive button UI element with text label.
 * Supports hover states, click callbacks, and keyboard selection.
 */
class Button {
public:
    Button(const std::string& label, const glm::vec2& position, const glm::vec2& size)
        : m_label(label)
        , m_position(position)
        , m_size(size)
        , m_hovered(false)
        , m_selected(false)
        , m_enabled(true) {}

    // Set callback function for button click
    void setOnClick(std::function<void()> callback) {
        m_onClickCallback = callback;
    }

    // Update button state with mouse position and click
    bool update(const glm::vec2& mousePos, bool mouseClicked) {
        if (!m_enabled) {
            m_hovered = false;
            return false;
        }

        // Always update hover state based on current mouse position
        m_hovered = isMouseOver(mousePos);

        if (m_hovered && mouseClicked) {
            if (m_onClickCallback) {
                m_onClickCallback();
            }
            return true;
        }
        return false;
    }

    // Activate button (for keyboard navigation)
    void activate() {
        if (m_enabled && m_onClickCallback) {
            m_onClickCallback();
        }
    }

    // Generate text vertices for rendering
    std::vector<TextVertex> generateTextVertices(
        TextRenderer& textRenderer,
        uint32_t screenWidth,
        uint32_t screenHeight,
        float guiScale = 1.0f
    ) const {
        // Choose style based on button state (color changes on hover, no bold)
        Style style;
        if (!m_enabled) {
            style = Style::darkGray();
        } else if (m_selected) {
            style = Style::yellow();
        } else if (m_hovered) {
            style = Style::yellow();  // Yellow on hover
        } else {
            style = Style::white();   // White by default
        }

        // Create text with style
        auto text = Text::literal(m_label, style);

        // Calculate centered position with larger scale
        float scale = 3.0f * guiScale;
        float textWidth = textRenderer.calculateTextWidth(text, scale);
        float textHeight = textRenderer.calculateTextHeight(text, scale);

        glm::vec2 textPos = m_position + glm::vec2(
            (m_size.x - textWidth) * 0.5f,
            (m_size.y - textHeight) * 0.5f
        );

        return textRenderer.generateVertices(text, textPos, scale, screenWidth, screenHeight);
    }

    // Getters/Setters
    void setPosition(const glm::vec2& pos) { m_position = pos; }
    void setSize(const glm::vec2& size) { m_size = size; }
    void setLabel(const std::string& label) { m_label = label; }
    void setSelected(bool selected) { m_selected = selected; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    void setHovered(bool hovered) { m_hovered = hovered; }

    const glm::vec2& getPosition() const { return m_position; }
    const glm::vec2& getSize() const { return m_size; }
    const std::string& getLabel() const { return m_label; }
    bool isSelected() const { return m_selected; }
    bool isHovered() const { return m_hovered; }
    bool isEnabled() const { return m_enabled; }

private:
    bool isMouseOver(const glm::vec2& mousePos) const {
        return mousePos.x >= m_position.x &&
               mousePos.x <= m_position.x + m_size.x &&
               mousePos.y >= m_position.y &&
               mousePos.y <= m_position.y + m_size.y;
    }

    std::string m_label;
    glm::vec2 m_position;
    glm::vec2 m_size;
    bool m_hovered;
    bool m_selected;
    bool m_enabled;
    std::function<void()> m_onClickCallback;
};

} // namespace VoxelEngine