#pragma once

#include "../text/Text.hpp"
#include "../text/TextRenderer.hpp"
#include "../text/Style.hpp"
#include <glm/glm.hpp>
#include <functional>
#include <string>

namespace FarHorizon {

/**
 * Interactive button UI element with text label.
 * Supports hover states, click callbacks, and keyboard selection.
 */
class Button {
public:
    Button(const std::string& label, const glm::vec2& position, const glm::vec2& size)
        : label_(label)
        , position_(position)
        , size_(size)
        , hovered_(false)
        , selected_(false)
        , enabled_(true) {}

    // Set callback function for button click
    void setOnClick(std::function<void()> callback) {
        onClickCallback_ = callback;
    }

    // Update button state with mouse position and click
    bool update(const glm::vec2& mousePos, bool mouseClicked) {
        if (!enabled_) {
            hovered_ = false;
            return false;
        }

        // Always update hover state based on current mouse position
        hovered_ = isMouseOver(mousePos);

        if (hovered_ && mouseClicked) {
            if (onClickCallback_) {
                onClickCallback_();
            }
            return true;
        }
        return false;
    }

    // Activate button (for keyboard navigation)
    void activate() {
        if (enabled_ && onClickCallback_) {
            onClickCallback_();
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
        if (!enabled_) {
            style = Style::darkGray();
        } else if (selected_) {
            style = Style::yellow();
        } else if (hovered_) {
            style = Style::yellow();  // Yellow on hover
        } else {
            style = Style::white();   // White by default
        }

        // Create text with style
        auto text = Text::literal(label_, style);

        // Calculate centered position with larger scale
        float scale = 3.0f * guiScale;
        float textWidth = textRenderer.calculateTextWidth(text, scale);
        float textHeight = textRenderer.calculateTextHeight(text, scale);

        glm::vec2 textPos = position_ + glm::vec2(
            (size_.x - textWidth) * 0.5f,
            (size_.y - textHeight) * 0.5f
        );

        return textRenderer.generateVertices(text, textPos, scale, screenWidth, screenHeight);
    }

    // Getters/Setters
    void setPosition(const glm::vec2& pos) { position_ = pos; }
    void setSize(const glm::vec2& size) { size_ = size; }
    void setLabel(const std::string& label) { label_ = label; }
    void setSelected(bool selected) { selected_ = selected; }
    void setEnabled(bool enabled) { enabled_ = enabled; }
    void setHovered(bool hovered) { hovered_ = hovered; }

    const glm::vec2& getPosition() const { return position_; }
    const glm::vec2& getSize() const { return size_; }
    const std::string& getLabel() const { return label_; }
    bool isSelected() const { return selected_; }
    bool isHovered() const { return hovered_; }
    bool isEnabled() const { return enabled_; }

private:
    bool isMouseOver(const glm::vec2& mousePos) const {
        return mousePos.x >= position_.x &&
               mousePos.x <= position_.x + size_.x &&
               mousePos.y >= position_.y &&
               mousePos.y <= position_.y + size_.y;
    }

    std::string label_;
    glm::vec2 position_;
    glm::vec2 size_;
    bool hovered_;
    bool selected_;
    bool enabled_;
    std::function<void()> onClickCallback_;
};

} // namespace FarHorizon