#pragma once

#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <memory>

namespace FarHorizon {

/**
 * Immutable text style inspired by Minecraft's style system.
 * Supports color, formatting effects, and shadow.
 */
class Style {
public:
    // Builder pattern for creating styles
    class Builder {
    public:
        Builder() = default;

        Builder& color(const glm::vec4& c) {
            color_ = c;
            return *this;
        }

        Builder& color(const glm::vec3& rgb) {
            color_ = glm::vec4(rgb, 1.0f);
            return *this;
        }

        Builder& bold(bool value = true) {
            bold_ = value;
            return *this;
        }

        Builder& italic(bool value = true) {
            italic_ = value;
            return *this;
        }

        Builder& underline(bool value = true) {
            underline_ = value;
            return *this;
        }

        Builder& strikethrough(bool value = true) {
            strikethrough_ = value;
            return *this;
        }

        Builder& obfuscated(bool value = true) {
            obfuscated_ = value;
            return *this;
        }

        Builder& shadow(bool value = true) {
            shadow_ = value;
            return *this;
        }

        Builder& shadowColor(const glm::vec4& c) {
            shadowColor_ = c;
            return *this;
        }

        Builder& font(const std::string& fontName) {
            font_ = fontName;
            return *this;
        }

        Style build() const {
            return Style(*this);
        }

    private:
        friend class Style;

        glm::vec4 color_{1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec4 shadowColor_{0.25f, 0.25f, 0.25f, 1.0f};
        std::string font_ = "default";
        bool bold_ = false;
        bool italic_ = false;
        bool underline_ = false;
        bool strikethrough_ = false;
        bool obfuscated_ = false;
        bool shadow_ = true;
    };

    // Default constructor - white color, default font, with shadow
    Style() : Style(Builder()) {}

    // Predefined colors (Minecraft-style)
    static Style black() { return Builder().color(glm::vec3(0.0f, 0.0f, 0.0f)).build(); }
    static Style darkBlue() { return Builder().color(glm::vec3(0.0f, 0.0f, 0.67f)).build(); }
    static Style darkGreen() { return Builder().color(glm::vec3(0.0f, 0.67f, 0.0f)).build(); }
    static Style darkAqua() { return Builder().color(glm::vec3(0.0f, 0.67f, 0.67f)).build(); }
    static Style darkRed() { return Builder().color(glm::vec3(0.67f, 0.0f, 0.0f)).build(); }
    static Style darkPurple() { return Builder().color(glm::vec3(0.67f, 0.0f, 0.67f)).build(); }
    static Style gold() { return Builder().color(glm::vec3(1.0f, 0.67f, 0.0f)).build(); }
    static Style gray() { return Builder().color(glm::vec3(0.67f, 0.67f, 0.67f)).build(); }
    static Style darkGray() { return Builder().color(glm::vec3(0.33f, 0.33f, 0.33f)).build(); }
    static Style blue() { return Builder().color(glm::vec3(0.33f, 0.33f, 1.0f)).build(); }
    static Style green() { return Builder().color(glm::vec3(0.33f, 1.0f, 0.33f)).build(); }
    static Style aqua() { return Builder().color(glm::vec3(0.33f, 1.0f, 1.0f)).build(); }
    static Style red() { return Builder().color(glm::vec3(1.0f, 0.33f, 0.33f)).build(); }
    static Style lightPurple() { return Builder().color(glm::vec3(1.0f, 0.33f, 1.0f)).build(); }
    static Style yellow() { return Builder().color(glm::vec3(1.0f, 1.0f, 0.33f)).build(); }
    static Style white() { return Builder().color(glm::vec3(1.0f, 1.0f, 1.0f)).build(); }

    // Getters
    const glm::vec4& getColor() const { return color_; }
    const glm::vec4& getShadowColor() const { return shadowColor_; }
    const std::string& getFont() const { return font_; }
    bool isBold() const { return bold_; }
    bool isItalic() const { return italic_; }
    bool isUnderline() const { return underline_; }
    bool isStrikethrough() const { return strikethrough_; }
    bool isObfuscated() const { return obfuscated_; }
    bool hasShadow() const { return shadow_; }

    // Modify current style (returns new style)
    Style withColor(const glm::vec4& color) const {
        Builder builder;
        builder.color_ = color;
        builder.shadowColor_ = shadowColor_;
        builder.font_ = font_;
        builder.bold_ = bold_;
        builder.italic_ = italic_;
        builder.underline_ = underline_;
        builder.strikethrough_ = strikethrough_;
        builder.obfuscated_ = obfuscated_;
        builder.shadow_ = shadow_;
        return Style(builder);
    }

    Style withBold(bool bold) const {
        Builder builder;
        builder.color_ = color_;
        builder.shadowColor_ = shadowColor_;
        builder.font_ = font_;
        builder.bold_ = bold;
        builder.italic_ = italic_;
        builder.underline_ = underline_;
        builder.strikethrough_ = strikethrough_;
        builder.obfuscated_ = obfuscated_;
        builder.shadow_ = shadow_;
        return Style(builder);
    }

private:
    explicit Style(const Builder& builder)
        : color_(builder.color_)
        , shadowColor_(builder.shadowColor_)
        , font_(builder.font_)
        , bold_(builder.bold_)
        , italic_(builder.italic_)
        , underline_(builder.underline_)
        , strikethrough_(builder.strikethrough_)
        , obfuscated_(builder.obfuscated_)
        , shadow_(builder.shadow_)
    {}

    glm::vec4 color_;
    glm::vec4 shadowColor_;
    std::string font_;
    bool bold_;
    bool italic_;
    bool underline_;
    bool strikethrough_;
    bool obfuscated_;
    bool shadow_;
};

} // namespace FarHorizon