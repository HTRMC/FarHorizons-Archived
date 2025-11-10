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
            m_color = c;
            return *this;
        }

        Builder& color(const glm::vec3& rgb) {
            m_color = glm::vec4(rgb, 1.0f);
            return *this;
        }

        Builder& bold(bool value = true) {
            m_bold = value;
            return *this;
        }

        Builder& italic(bool value = true) {
            m_italic = value;
            return *this;
        }

        Builder& underline(bool value = true) {
            m_underline = value;
            return *this;
        }

        Builder& strikethrough(bool value = true) {
            m_strikethrough = value;
            return *this;
        }

        Builder& obfuscated(bool value = true) {
            m_obfuscated = value;
            return *this;
        }

        Builder& shadow(bool value = true) {
            m_shadow = value;
            return *this;
        }

        Builder& shadowColor(const glm::vec4& c) {
            m_shadowColor = c;
            return *this;
        }

        Builder& font(const std::string& fontName) {
            m_font = fontName;
            return *this;
        }

        Style build() const {
            return Style(*this);
        }

    private:
        friend class Style;

        glm::vec4 m_color{1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec4 m_shadowColor{0.25f, 0.25f, 0.25f, 1.0f};
        std::string m_font = "default";
        bool m_bold = false;
        bool m_italic = false;
        bool m_underline = false;
        bool m_strikethrough = false;
        bool m_obfuscated = false;
        bool m_shadow = true;
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
    const glm::vec4& getColor() const { return m_color; }
    const glm::vec4& getShadowColor() const { return m_shadowColor; }
    const std::string& getFont() const { return m_font; }
    bool isBold() const { return m_bold; }
    bool isItalic() const { return m_italic; }
    bool isUnderline() const { return m_underline; }
    bool isStrikethrough() const { return m_strikethrough; }
    bool isObfuscated() const { return m_obfuscated; }
    bool hasShadow() const { return m_shadow; }

    // Modify current style (returns new style)
    Style withColor(const glm::vec4& color) const {
        Builder builder;
        builder.m_color = color;
        builder.m_shadowColor = m_shadowColor;
        builder.m_font = m_font;
        builder.m_bold = m_bold;
        builder.m_italic = m_italic;
        builder.m_underline = m_underline;
        builder.m_strikethrough = m_strikethrough;
        builder.m_obfuscated = m_obfuscated;
        builder.m_shadow = m_shadow;
        return Style(builder);
    }

    Style withBold(bool bold) const {
        Builder builder;
        builder.m_color = m_color;
        builder.m_shadowColor = m_shadowColor;
        builder.m_font = m_font;
        builder.m_bold = bold;
        builder.m_italic = m_italic;
        builder.m_underline = m_underline;
        builder.m_strikethrough = m_strikethrough;
        builder.m_obfuscated = m_obfuscated;
        builder.m_shadow = m_shadow;
        return Style(builder);
    }

private:
    explicit Style(const Builder& builder)
        : m_color(builder.m_color)
        , m_shadowColor(builder.m_shadowColor)
        , m_font(builder.m_font)
        , m_bold(builder.m_bold)
        , m_italic(builder.m_italic)
        , m_underline(builder.m_underline)
        , m_strikethrough(builder.m_strikethrough)
        , m_obfuscated(builder.m_obfuscated)
        , m_shadow(builder.m_shadow)
    {}

    glm::vec4 m_color;
    glm::vec4 m_shadowColor;
    std::string m_font;
    bool m_bold;
    bool m_italic;
    bool m_underline;
    bool m_strikethrough;
    bool m_obfuscated;
    bool m_shadow;
};

} // namespace FarHorizon