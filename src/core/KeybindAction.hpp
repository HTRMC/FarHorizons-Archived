#pragma once

#include <string>
#include <magic_enum/magic_enum.hpp>

namespace FarHorizon {

/**
 * Enum for keybind actions (faster than string lookups)
 */
enum class KeybindAction {
    Attack,
    Use,
    Forward,
    Left,
    Back,
    Right,
    Jump,
    Sneak,
    Sprint,
    Chat,
    Command,
    Screenshot,
    TogglePerspective,
    Fullscreen
};

/**
 * Convert KeybindAction enum to settings file key string (procedural using magic_enum)
 * Example: KeybindAction::Forward -> "key.forward"
 */
inline std::string keybindActionToString(KeybindAction action) {
    auto enumName = magic_enum::enum_name(action);
    std::string result = "key.";

    // Convert PascalCase to camelCase (e.g., "TogglePerspective" -> "togglePerspective")
    if (!enumName.empty()) {
        result += static_cast<char>(std::tolower(enumName[0]));
        result += enumName.substr(1);
    }

    return result;
}

} // namespace FarHorizon