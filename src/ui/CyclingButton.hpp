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
        : label_(label)
        , position_(position)
        , width_(width)
        , options_(options)
        , currentIndex_(currentIndex)
        , scale_(scale)
        , hovered_(false)
        , selected_(false)
        , enabled_(true) {

        height_ = 60.0f * scale; // Height for the button

        if (!options_.empty() && currentIndex_ >= options_.size()) {
            currentIndex_ = 0;
        }
    }

    // Set callback function for when option changes
    void setOnChange(std::function<void(const std::string&)> callback) {
        onChangeCallback_ = callback;
    }

    // Update button state with mouse input
    bool update(const glm::vec2& mousePos, bool mouseClicked) {
        if (!enabled_) {
            hovered_ = false;
            return false;
        }

        // Update hover state
        hovered_ = isMouseOver(mousePos);

        if (hovered_ && mouseClicked) {
            cycleNext();
            return true;
        }
        return false;
    }

    // Cycle to the next option (wraps around)
    void cycleNext() {
        if (options_.empty()) return;

        currentIndex_ = (currentIndex_ + 1) % options_.size();

        if (onChangeCallback_) {
            onChangeCallback_(options_[currentIndex_]);
        }
    }

    // Activate button (for keyboard navigation)
    void activate() {
        if (enabled_) {
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
        if (!enabled_) {
            labelStyle = Style::darkGray();
        } else if (selected_) {
            labelStyle = Style::yellow();
        } else if (hovered_) {
            labelStyle = Style::yellow();
        } else {
            labelStyle = Style::white();
        }

        // Build the display text: "Label: CurrentOption"
        std::string displayText = label_ + ": ";
        std::string currentOption = options_.empty() ? "None" : options_[currentIndex_];

        auto text = Text::literal(displayText + currentOption, labelStyle);

        // Calculate centered position
        float scale = 2.5f * guiScale;
        float textWidth = textRenderer.calculateTextWidth(text, scale);
        float textHeight = textRenderer.calculateTextHeight(text, scale);

        glm::vec2 textPos = position_ + glm::vec2(
            (width_ - textWidth) * 0.5f,
            (height_ - textHeight) * 0.5f
        );

        return textRenderer.generateVertices(text, textPos, scale, screenWidth, screenHeight);
    }

    // Getters/Setters
    void setPosition(const glm::vec2& pos) { position_ = pos; }
    void setSelected(bool selected) { selected_ = selected; }
    void setEnabled(bool enabled) { enabled_ = enabled; }
    void setHovered(bool hovered) { hovered_ = hovered; }

    const glm::vec2& getPosition() const { return position_; }
    float getHeight() const { return height_; }
    bool isSelected() const { return selected_; }
    bool isHovered() const { return hovered_; }
    bool isEnabled() const { return enabled_; }

    const std::string& getCurrentOption() const {
        return options_.empty() ? "" : options_[currentIndex_];
    }

    size_t getCurrentIndex() const { return currentIndex_; }

    void setOptions(const std::vector<std::string>& options, size_t index = 0) {
        options_ = options;
        currentIndex_ = index;
        if (!options_.empty() && currentIndex_ >= options_.size()) {
            currentIndex_ = 0;
        }
    }

    void setCurrentIndex(size_t index) {
        if (!options_.empty() && index < options_.size()) {
            currentIndex_ = index;
        }
    }

private:
    bool isMouseOver(const glm::vec2& mousePos) const {
        return mousePos.x >= position_.x &&
               mousePos.x <= position_.x + width_ &&
               mousePos.y >= position_.y &&
               mousePos.y <= position_.y + height_;
    }

    std::string label_;
    glm::vec2 position_;
    float width_;
    float height_;
    std::vector<std::string> options_;
    size_t currentIndex_;
    float scale_;
    bool hovered_;
    bool selected_;
    bool enabled_;
    std::function<void(const std::string&)> onChangeCallback_;
};

} // namespace FarHorizon