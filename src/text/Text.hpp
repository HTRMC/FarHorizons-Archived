#pragma once

#include "Style.hpp"
#include <string>
#include <vector>
#include <memory>

namespace FarHorizon {

/**
 * Represents a styled text component that can be composed of multiple segments.
 * Similar to Minecraft's Text interface.
 */
class Text {
public:
    struct Segment {
        std::string content;
        Style style;

        Segment(const std::string& text, const Style& s = Style())
            : content(text), style(s) {}
    };

    Text() = default;
    explicit Text(const std::string& content, const Style& style = Style()) {
        m_segments.emplace_back(content, style);
    }

    // Builder-style methods for composing text
    Text& append(const std::string& content, const Style& style = Style()) {
        m_segments.emplace_back(content, style);
        return *this;
    }

    Text& append(const Text& other) {
        m_segments.insert(m_segments.end(), other.m_segments.begin(), other.m_segments.end());
        return *this;
    }

    // Apply style to all segments
    Text& withStyle(const Style& style) {
        for (auto& segment : m_segments) {
            segment.style = style;
        }
        return *this;
    }

    // Get all segments
    const std::vector<Segment>& getSegments() const { return m_segments; }

    // Get plain string (without styling)
    std::string getString() const {
        std::string result;
        for (const auto& segment : m_segments) {
            result += segment.content;
        }
        return result;
    }

    // Create literal text
    static Text literal(const std::string& content, const Style& style = Style()) {
        return Text(content, style);
    }

    // Parse legacy formatting codes (§ codes like Minecraft)
    static Text parseLegacy(const std::string& text) {
        static constexpr const char* FORMATTING_CODE_PREFIX = "§";
        Text result;
        std::string current;
        Style currentStyle;

        for (size_t i = 0; i < text.length(); ++i) {
            if (i + 2 < text.length() && text.substr(i, 2) == FORMATTING_CODE_PREFIX) {
                // Save current segment if not empty
                if (!current.empty()) {
                    result.append(current, currentStyle);
                    current.clear();
                }

                // Skip the § character (2 bytes in UTF-8) and get the code
                i += 2;
                char code = text[i];
                switch (code) {
                    // Colors
                    case '0': currentStyle = Style::black(); break;
                    case '1': currentStyle = Style::darkBlue(); break;
                    case '2': currentStyle = Style::darkGreen(); break;
                    case '3': currentStyle = Style::darkAqua(); break;
                    case '4': currentStyle = Style::darkRed(); break;
                    case '5': currentStyle = Style::darkPurple(); break;
                    case '6': currentStyle = Style::gold(); break;
                    case '7': currentStyle = Style::gray(); break;
                    case '8': currentStyle = Style::darkGray(); break;
                    case '9': currentStyle = Style::blue(); break;
                    case 'a': currentStyle = Style::green(); break;
                    case 'b': currentStyle = Style::aqua(); break;
                    case 'c': currentStyle = Style::red(); break;
                    case 'd': currentStyle = Style::lightPurple(); break;
                    case 'e': currentStyle = Style::yellow(); break;
                    case 'f': currentStyle = Style::white(); break;

                    // Formatting
                    case 'l': currentStyle = currentStyle.withBold(true); break;
                    case 'r': currentStyle = Style(); break; // Reset

                    default:
                        current += FORMATTING_CODE_PREFIX;
                        current += code;
                        break;
                }
            } else {
                current += text[i];
            }
        }

        // Add remaining text
        if (!current.empty()) {
            result.append(current, currentStyle);
        }

        return result;
    }

private:
    std::vector<Segment> m_segments;
};

} // namespace FarHorizon