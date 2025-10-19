#pragma once

#include "InputTypes.hpp"
#include <variant>
#include <string>

namespace VoxelEngine {

// Event types for the input system
enum class InputEventType {
    KeyPressed,
    KeyReleased,
    KeyRepeat,
    MouseButtonPressed,
    MouseButtonReleased,
    MouseMoved,
    MouseScrolled,
    GamepadButtonPressed,
    GamepadButtonReleased,
    GamepadAxisMoved,
    GamepadConnected,
    GamepadDisconnected
};

// Base event data
struct InputEventData {
    InputEventType type;
    double timestamp;

    InputEventData(InputEventType t, double ts) : type(t), timestamp(ts) {}
    virtual ~InputEventData() = default;
};

// Keyboard events
struct KeyEventData : InputEventData {
    KeyCode key;
    int scancode;
    int mods;

    KeyEventData(InputEventType type, KeyCode k, int sc, int m, double ts)
        : InputEventData(type, ts), key(k), scancode(sc), mods(m) {}
};

// Mouse button events
struct MouseButtonEventData : InputEventData {
    MouseButton button;
    int mods;
    double mouseX;
    double mouseY;

    MouseButtonEventData(InputEventType type, MouseButton btn, int m, double x, double y, double ts)
        : InputEventData(type, ts), button(btn), mods(m), mouseX(x), mouseY(y) {}
};

// Mouse moved event
struct MouseMovedEventData : InputEventData {
    double x;
    double y;
    double deltaX;
    double deltaY;

    MouseMovedEventData(double px, double py, double dx, double dy, double ts)
        : InputEventData(InputEventType::MouseMoved, ts), x(px), y(py), deltaX(dx), deltaY(dy) {}
};

// Mouse scroll event
struct MouseScrollEventData : InputEventData {
    double xOffset;
    double yOffset;

    MouseScrollEventData(double xOff, double yOff, double ts)
        : InputEventData(InputEventType::MouseScrolled, ts), xOffset(xOff), yOffset(yOff) {}
};

// Gamepad button events
struct GamepadButtonEventData : InputEventData {
    int joystickID;
    GamepadButton button;

    GamepadButtonEventData(InputEventType type, int jid, GamepadButton btn, double ts)
        : InputEventData(type, ts), joystickID(jid), button(btn) {}
};

// Gamepad axis event
struct GamepadAxisEventData : InputEventData {
    int joystickID;
    GamepadAxis axis;
    float value;
    float previousValue;

    GamepadAxisEventData(int jid, GamepadAxis ax, float val, float prev, double ts)
        : InputEventData(InputEventType::GamepadAxisMoved, ts)
        , joystickID(jid), axis(ax), value(val), previousValue(prev) {}
};

// Gamepad connection events
struct GamepadConnectionEventData : InputEventData {
    int joystickID;
    std::string name;

    GamepadConnectionEventData(InputEventType type, int jid, const std::string& n, double ts)
        : InputEventData(type, ts), joystickID(jid), name(n) {}
};

// Variant to hold any event type
using InputEvent = std::variant<
    KeyEventData,
    MouseButtonEventData,
    MouseMovedEventData,
    MouseScrollEventData,
    GamepadButtonEventData,
    GamepadAxisEventData,
    GamepadConnectionEventData
>;

// Helper to get timestamp from any event
inline double getEventTimestamp(const InputEvent& event) {
    return std::visit([](const auto& e) { return e.timestamp; }, event);
}

// Helper to get event type from any event
inline InputEventType getEventType(const InputEvent& event) {
    return std::visit([](const auto& e) { return e.type; }, event);
}

} // namespace VoxelEngine