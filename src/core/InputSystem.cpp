#include "InputSystem.hpp"
#include <algorithm>
#include <print>

#include <magic_enum/magic_enum.hpp>

namespace VoxelEngine {

// Static member initialization
GLFWwindow* InputSystem::s_window = nullptr;
InputQueue InputSystem::s_eventQueue;
std::vector<InputEvent> InputSystem::s_processedEvents;

uint32_t InputSystem::s_nextListenerID = 1;
std::unordered_map<uint32_t, std::pair<InputEventType, InputEventCallback>> InputSystem::s_listeners;

std::array<bool, static_cast<size_t>(KeyCode::MaxKeys)> InputSystem::s_keys = {};
std::array<bool, static_cast<size_t>(KeyCode::MaxKeys)> InputSystem::s_keysPrevious = {};

std::array<bool, static_cast<size_t>(MouseButton::MaxButtons)> InputSystem::s_mouseButtons = {};
std::array<bool, static_cast<size_t>(MouseButton::MaxButtons)> InputSystem::s_mouseButtonsPrevious = {};
glm::vec2 InputSystem::s_mousePosition = glm::vec2(0.0f);
glm::vec2 InputSystem::s_mousePositionPrevious = glm::vec2(0.0f);
glm::vec2 InputSystem::s_mouseDelta = glm::vec2(0.0f);
glm::vec2 InputSystem::s_mouseScroll = glm::vec2(0.0f);

std::array<InputSystem::GamepadState, 4> InputSystem::s_gamepads = {};
float InputSystem::s_analogDeadzone = 0.15f;
MouseCapture* InputSystem::s_mouseCapture = nullptr;

void InputSystem::init(GLFWwindow* window) {
    s_window = window;

    // Set up GLFW callbacks
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetJoystickCallback(joystickCallback);

    // Initialize gamepads
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
        if (glfwJoystickPresent(jid) && glfwJoystickIsGamepad(jid)) {
            s_gamepads[jid].connected = true;
#ifndef NDEBUG
            std::println("[InputSystem] Gamepad {} connected: {}", jid, glfwGetGamepadName(jid));
#endif
        }
    }

    std::println("[InputSystem] Initialized with event queue");
}

void InputSystem::shutdown() {
    s_eventQueue.clear();
    s_listeners.clear();
}

void InputSystem::processEvents() {
    // Save previous frame state
    s_keysPrevious = s_keys;
    s_mouseButtonsPrevious = s_mouseButtons;
    s_mousePositionPrevious = s_mousePosition;
    s_mouseScroll = glm::vec2(0.0f);
    s_mouseDelta = glm::vec2(0.0f);

    for (auto& gamepad : s_gamepads) {
        gamepad.buttonsPrevious = gamepad.buttons;
        gamepad.axesPrevious = gamepad.axes;
    }

    // Poll gamepad state and generate events (GLFW has no gamepad callbacks)
    for (int jid = 0; jid < 4; ++jid) {
        if (!s_gamepads[jid].connected) continue;

        GLFWgamepadstate state;
        if (glfwGetGamepadState(GLFW_JOYSTICK_1 + jid, &state)) {
            // Check button changes (compare current vs previous)
            for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; ++i) {
                bool current = (state.buttons[i] == GLFW_PRESS);
                if (current != s_gamepads[jid].buttons[i]) {
                    auto type = current ? InputEventType::GamepadButtonPressed : InputEventType::GamepadButtonReleased;
                    GamepadButtonEventData event(type, jid, static_cast<GamepadButton>(i), glfwGetTime());
                    s_eventQueue.push(event);
#ifndef NDEBUG
                    auto enumName = magic_enum::enum_name(static_cast<GamepadButton>(i));
                    std::println("[InputSystem] Gamepad {} button {}: {}", jid, enumName,
                        current ? "pressed" : "released");
#endif
                }
            }

            // Check axis changes
            for (int i = 0; i <= GLFW_GAMEPAD_AXIS_LAST; ++i) {
                float current = state.axes[i];
                float previous = s_gamepads[jid].axes[i];

                // Only generate event if change is significant (> 0.01)
                if (std::abs(current - previous) > 0.01f) {
                    GamepadAxisEventData event(jid, static_cast<GamepadAxis>(i), current, previous, glfwGetTime());
                    s_eventQueue.push(event);
#ifndef NDEBUG
                    auto enumName = magic_enum::enum_name(static_cast<GamepadAxis>(i));
                    std::println("[InputSystem] Gamepad {} axis {}: {:.3f} (was {:.3f})",
                        jid, enumName, current, previous);
#endif
                }
            }
        }
    }

    // Get all events from queue (thread-safe)
    s_processedEvents = s_eventQueue.pollEvents();

    // Process each event
    for (const auto& event : s_processedEvents) {
        std::visit([](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, KeyEventData>) {
                processKeyEvent(e);
            } else if constexpr (std::is_same_v<T, MouseButtonEventData>) {
                processMouseButtonEvent(e);
            } else if constexpr (std::is_same_v<T, MouseMovedEventData>) {
                processMouseMovedEvent(e);
            } else if constexpr (std::is_same_v<T, MouseScrollEventData>) {
                processMouseScrollEvent(e);
            } else if constexpr (std::is_same_v<T, GamepadButtonEventData>) {
                processGamepadButtonEvent(e);
            } else if constexpr (std::is_same_v<T, GamepadAxisEventData>) {
                processGamepadAxisEvent(e);
            }
        }, event);

        // Notify listeners
        notifyListeners(event);
    }
}

uint32_t InputSystem::addEventListener(InputEventType type, InputEventCallback callback) {
    uint32_t id = s_nextListenerID++;
    s_listeners[id] = {type, callback};
    return id;
}

void InputSystem::removeEventListener(uint32_t listenerID) {
    s_listeners.erase(listenerID);
}

// Keyboard queries
bool InputSystem::isKeyPressed(KeyCode key) {
    int keyCode = static_cast<int>(key);
    if (keyCode < 0 || keyCode >= static_cast<int>(KeyCode::MaxKeys)) return false;
    return s_keys[keyCode];
}

bool InputSystem::isKeyDown(KeyCode key) {
    int keyCode = static_cast<int>(key);
    if (keyCode < 0 || keyCode >= static_cast<int>(KeyCode::MaxKeys)) return false;
    return s_keys[keyCode] && !s_keysPrevious[keyCode];
}

bool InputSystem::isKeyReleased(KeyCode key) {
    int keyCode = static_cast<int>(key);
    if (keyCode < 0 || keyCode >= static_cast<int>(KeyCode::MaxKeys)) return false;
    return !s_keys[keyCode] && s_keysPrevious[keyCode];
}

// Mouse queries
bool InputSystem::isMouseButtonPressed(MouseButton button) {
    int buttonCode = static_cast<int>(button);
    if (buttonCode < 0 || buttonCode >= static_cast<int>(MouseButton::MaxButtons)) return false;
    return s_mouseButtons[buttonCode];
}

bool InputSystem::isMouseButtonDown(MouseButton button) {
    int buttonCode = static_cast<int>(button);
    if (buttonCode < 0 || buttonCode >= static_cast<int>(MouseButton::MaxButtons)) return false;
    return s_mouseButtons[buttonCode] && !s_mouseButtonsPrevious[buttonCode];
}

bool InputSystem::isMouseButtonReleased(MouseButton button) {
    int buttonCode = static_cast<int>(button);
    if (buttonCode < 0 || buttonCode >= static_cast<int>(MouseButton::MaxButtons)) return false;
    return !s_mouseButtons[buttonCode] && s_mouseButtonsPrevious[buttonCode];
}

glm::vec2 InputSystem::getMousePosition() {
    return s_mousePosition;
}

glm::vec2 InputSystem::getMouseDelta() {
    return s_mouseDelta;
}

glm::vec2 InputSystem::getMouseScroll() {
    return s_mouseScroll;
}

// Gamepad queries
bool InputSystem::isGamepadConnected(int joystickID) {
    if (joystickID < 0 || joystickID >= 4) return false;
    return s_gamepads[joystickID].connected;
}

bool InputSystem::isGamepadButtonPressed(GamepadButton button, int joystickID) {
    if (joystickID < 0 || joystickID >= 4 || !s_gamepads[joystickID].connected) return false;
    int buttonCode = static_cast<int>(button);
    if (buttonCode < 0 || buttonCode >= static_cast<int>(GamepadButton::MaxButtons)) return false;
    return s_gamepads[joystickID].buttons[buttonCode];
}

bool InputSystem::isGamepadButtonDown(GamepadButton button, int joystickID) {
    if (joystickID < 0 || joystickID >= 4 || !s_gamepads[joystickID].connected) return false;
    int buttonCode = static_cast<int>(button);
    if (buttonCode < 0 || buttonCode >= static_cast<int>(GamepadButton::MaxButtons)) return false;
    return s_gamepads[joystickID].buttons[buttonCode] && !s_gamepads[joystickID].buttonsPrevious[buttonCode];
}

bool InputSystem::isGamepadButtonReleased(GamepadButton button, int joystickID) {
    if (joystickID < 0 || joystickID >= 4 || !s_gamepads[joystickID].connected) return false;
    int buttonCode = static_cast<int>(button);
    if (buttonCode < 0 || buttonCode >= static_cast<int>(GamepadButton::MaxButtons)) return false;
    return !s_gamepads[joystickID].buttons[buttonCode] && s_gamepads[joystickID].buttonsPrevious[buttonCode];
}

float InputSystem::getGamepadAxis(GamepadAxis axis, int joystickID) {
    if (joystickID < 0 || joystickID >= 4 || !s_gamepads[joystickID].connected) return 0.0f;
    int axisCode = static_cast<int>(axis);
    if (axisCode < 0 || axisCode >= static_cast<int>(GamepadAxis::MaxAxes)) return 0.0f;
    return applyDeadzone(s_gamepads[joystickID].axes[axisCode]);
}

glm::vec2 InputSystem::getGamepadLeftStick(int joystickID) {
    return applyDeadzone(glm::vec2(
        getGamepadAxis(GamepadAxis::LeftX, joystickID),
        getGamepadAxis(GamepadAxis::LeftY, joystickID)
    ));
}

glm::vec2 InputSystem::getGamepadRightStick(int joystickID) {
    return applyDeadzone(glm::vec2(
        getGamepadAxis(GamepadAxis::RightX, joystickID),
        getGamepadAxis(GamepadAxis::RightY, joystickID)
    ));
}

// Modifiers
bool InputSystem::isShiftPressed() {
    return isKeyPressed(KeyCode::LeftShift) || isKeyPressed(KeyCode::RightShift);
}

bool InputSystem::isControlPressed() {
    return isKeyPressed(KeyCode::LeftControl) || isKeyPressed(KeyCode::RightControl);
}

bool InputSystem::isAltPressed() {
    return isKeyPressed(KeyCode::LeftAlt) || isKeyPressed(KeyCode::RightAlt);
}

bool InputSystem::isSuperPressed() {
    return isKeyPressed(KeyCode::LeftSuper) || isKeyPressed(KeyCode::RightSuper);
}

// GLFW Callbacks (input thread)
void InputSystem::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key < 0 || key >= static_cast<int>(KeyCode::MaxKeys)) return;

    InputEventType type = InputEventType::KeyPressed;
    if (action == GLFW_RELEASE) {
        type = InputEventType::KeyReleased;
    } else if (action == GLFW_REPEAT) {
        type = InputEventType::KeyRepeat;
    }

    KeyEventData eventData(type, static_cast<KeyCode>(key), scancode, mods, glfwGetTime());
    s_eventQueue.push(eventData);

#ifndef NDEBUG
    if (action != GLFW_REPEAT) {
        auto enumName = magic_enum::enum_name(static_cast<KeyCode>(key));
        std::println("[InputSystem] Key {}: {} (code: {})",
            action == GLFW_PRESS ? "pressed" : "released", enumName, key);
    }
#endif
}

void InputSystem::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button < 0 || button >= static_cast<int>(MouseButton::MaxButtons)) return;

    double x, y;
    glfwGetCursorPos(window, &x, &y);

    InputEventType type = (action == GLFW_PRESS) ? InputEventType::MouseButtonPressed : InputEventType::MouseButtonReleased;
    MouseButtonEventData eventData(type, static_cast<MouseButton>(button), mods, x, y, glfwGetTime());
    s_eventQueue.push(eventData);
}

void InputSystem::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    double dx = xpos - s_mousePosition.x;
    double dy = ypos - s_mousePosition.y;

    // Forward to mouse capture system if available
    if (s_mouseCapture) {
        s_mouseCapture->updateCursorPosition(xpos, ypos);
    }

    MouseMovedEventData eventData(xpos, ypos, dx, dy, glfwGetTime());
    s_eventQueue.push(eventData);
}

void InputSystem::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    MouseScrollEventData eventData(xoffset, yoffset, glfwGetTime());
    s_eventQueue.push(eventData);
}

void InputSystem::joystickCallback(int jid, int event) {
    if (jid < 0 || jid >= 4) return;

    if (event == GLFW_CONNECTED && glfwJoystickIsGamepad(jid)) {
        s_gamepads[jid].connected = true;
        GamepadConnectionEventData eventData(InputEventType::GamepadConnected, jid,
            glfwGetGamepadName(jid), glfwGetTime());
        s_eventQueue.push(eventData);

#ifndef NDEBUG
        std::println("[InputSystem] Gamepad {} connected: {}", jid, glfwGetGamepadName(jid));
#endif
    } else if (event == GLFW_DISCONNECTED) {
        s_gamepads[jid].connected = false;
        GamepadConnectionEventData eventData(InputEventType::GamepadDisconnected, jid, "", glfwGetTime());
        s_eventQueue.push(eventData);

#ifndef NDEBUG
        std::println("[InputSystem] Gamepad {} disconnected", jid);
#endif
    }
}

// Event processors
void InputSystem::processKeyEvent(const KeyEventData& event) {
    int keyCode = static_cast<int>(event.key);
    if (keyCode < 0 || keyCode >= static_cast<int>(KeyCode::MaxKeys)) return;

    if (event.type == InputEventType::KeyPressed || event.type == InputEventType::KeyRepeat) {
        s_keys[keyCode] = true;
    } else if (event.type == InputEventType::KeyReleased) {
        s_keys[keyCode] = false;
    }
}

void InputSystem::processMouseButtonEvent(const MouseButtonEventData& event) {
    int buttonCode = static_cast<int>(event.button);
    if (buttonCode < 0 || buttonCode >= static_cast<int>(MouseButton::MaxButtons)) return;

    s_mouseButtons[buttonCode] = (event.type == InputEventType::MouseButtonPressed);
}

void InputSystem::processMouseMovedEvent(const MouseMovedEventData& event) {
    s_mousePosition = glm::vec2(static_cast<float>(event.x), static_cast<float>(event.y));
    s_mouseDelta += glm::vec2(static_cast<float>(event.deltaX), static_cast<float>(event.deltaY));
}

void InputSystem::processMouseScrollEvent(const MouseScrollEventData& event) {
    s_mouseScroll += glm::vec2(static_cast<float>(event.xOffset), static_cast<float>(event.yOffset));
}

void InputSystem::processGamepadButtonEvent(const GamepadButtonEventData& event) {
    if (event.joystickID < 0 || event.joystickID >= 4) return;
    int buttonCode = static_cast<int>(event.button);
    if (buttonCode < 0 || buttonCode >= static_cast<int>(GamepadButton::MaxButtons)) return;

    s_gamepads[event.joystickID].buttons[buttonCode] = (event.type == InputEventType::GamepadButtonPressed);
}

void InputSystem::processGamepadAxisEvent(const GamepadAxisEventData& event) {
    if (event.joystickID < 0 || event.joystickID >= 4) return;
    int axisCode = static_cast<int>(event.axis);
    if (axisCode < 0 || axisCode >= static_cast<int>(GamepadAxis::MaxAxes)) return;

    s_gamepads[event.joystickID].axes[axisCode] = event.value;
}

void InputSystem::notifyListeners(const InputEvent& event) {
    InputEventType type = getEventType(event);

    for (const auto& [id, listener] : s_listeners) {
        if (listener.first == type) {
            listener.second(event);
        }
    }
}

float InputSystem::applyDeadzone(float value) {
    if (std::abs(value) < s_analogDeadzone) {
        return 0.0f;
    }

    // Remap from [deadzone, 1] to [0, 1]
    float sign = (value > 0.0f) ? 1.0f : -1.0f;
    float absValue = std::abs(value);
    return sign * ((absValue - s_analogDeadzone) / (1.0f - s_analogDeadzone));
}

glm::vec2 InputSystem::applyDeadzone(glm::vec2 value) {
    float magnitude = glm::length(value);

    if (magnitude < s_analogDeadzone) {
        return glm::vec2(0.0f);
    }

    // Radial deadzone with remapping
    glm::vec2 direction = value / magnitude;
    float newMagnitude = (magnitude - s_analogDeadzone) / (1.0f - s_analogDeadzone);
    return direction * std::min(newMagnitude, 1.0f);
}

KeyCode InputSystem::stringToKeyCode(const std::string& keybind) {
    // Parse keybind string procedurally using magic_enum
    // Format: "key.keyboard.w" or "key.keyboard.left.shift"

    if (keybind.substr(0, 13) != "key.keyboard.") {
        return KeyCode::Unknown;
    }

    std::string keyName = keybind.substr(13); // Remove "key.keyboard." prefix

    // Handle special cases with dots (e.g., "left.shift" -> "LeftShift")
    size_t dotPos = keyName.find('.');
    if (dotPos != std::string::npos) {
        // Capitalize first letter of each part
        keyName[0] = std::toupper(keyName[0]);
        keyName[dotPos + 1] = std::toupper(keyName[dotPos + 1]);
        keyName.erase(dotPos, 1); // Remove the dot
    } else {
        // Single word: capitalize first letter
        keyName[0] = std::toupper(keyName[0]);
    }

    // Try to parse using magic_enum
    auto result = magic_enum::enum_cast<KeyCode>(keyName);
    if (result.has_value()) {
        return result.value();
    }

    return KeyCode::Unknown;
}

MouseButton InputSystem::stringToMouseButton(const std::string& keybind) {
    static const std::unordered_map<std::string, MouseButton> mouseMap = {
        {"key.mouse.left", MouseButton::Left},
        {"key.mouse.right", MouseButton::Right},
        {"key.mouse.middle", MouseButton::Middle}
    };

    auto it = mouseMap.find(keybind);
    if (it != mouseMap.end()) {
        return it->second;
    }
    return MouseButton::MaxButtons; // Invalid
}

} // namespace VoxelEngine