#include "Input.hpp"
#include <algorithm>
#include <print>

#define MAGIC_ENUM_RANGE_MIN 0
#define MAGIC_ENUM_RANGE_MAX 512
#include <magic_enum/magic_enum.hpp>

namespace VoxelEngine {

// Static member initialization
GLFWwindow* Input::s_window = nullptr;

std::array<bool, static_cast<size_t>(KeyCode::MaxKeys)> Input::s_keys = {};
std::array<bool, static_cast<size_t>(KeyCode::MaxKeys)> Input::s_keysPrevious = {};

std::array<bool, static_cast<size_t>(MouseButton::MaxButtons)> Input::s_mouseButtons = {};
std::array<bool, static_cast<size_t>(MouseButton::MaxButtons)> Input::s_mouseButtonsPrevious = {};
glm::vec2 Input::s_mousePosition = glm::vec2(0.0f);
glm::vec2 Input::s_mousePositionPrevious = glm::vec2(0.0f);
glm::vec2 Input::s_mouseScroll = glm::vec2(0.0f);

std::array<bool, static_cast<size_t>(GamepadButton::MaxButtons)> Input::s_gamepadButtons = {};
std::array<bool, static_cast<size_t>(GamepadButton::MaxButtons)> Input::s_gamepadButtonsPrevious = {};
std::array<float, static_cast<size_t>(GamepadAxis::MaxAxes)> Input::s_gamepadAxes = {};

void Input::init(GLFWwindow* window) {
    s_window = window;

    // Set up GLFW callbacks
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, scrollCallback);

    // Initialize states
    s_keys.fill(false);
    s_keysPrevious.fill(false);
    s_mouseButtons.fill(false);
    s_mouseButtonsPrevious.fill(false);
    s_gamepadButtons.fill(false);
    s_gamepadButtonsPrevious.fill(false);
    s_gamepadAxes.fill(0.0f);
}

void Input::update() {
    // Update previous states
    s_keysPrevious = s_keys;
    s_mouseButtonsPrevious = s_mouseButtons;
    s_gamepadButtonsPrevious = s_gamepadButtons;

    // Update mouse position
    s_mousePositionPrevious = s_mousePosition;
    double xpos, ypos;
    glfwGetCursorPos(s_window, &xpos, &ypos);
    s_mousePosition = glm::vec2(static_cast<float>(xpos), static_cast<float>(ypos));

    // Reset scroll
    s_mouseScroll = glm::vec2(0.0f);

    // Poll keyboard state directly (for keys held down)
    for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key) {
        int state = glfwGetKey(s_window, key);
        s_keys[key] = (state == GLFW_PRESS || state == GLFW_REPEAT);
    }

    // Poll mouse button state
    for (int button = GLFW_MOUSE_BUTTON_1; button <= GLFW_MOUSE_BUTTON_LAST; ++button) {
        int state = glfwGetMouseButton(s_window, button);
        s_mouseButtons[button] = (state == GLFW_PRESS);
    }

    // Poll gamepad state
    if (isGamepadConnected()) {
        GLFWgamepadstate state;
        if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state)) {
            // Update button states
            for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; ++i) {
                s_gamepadButtons[i] = (state.buttons[i] == GLFW_PRESS);
            }

            // Update axis states
            for (int i = 0; i <= GLFW_GAMEPAD_AXIS_LAST; ++i) {
                s_gamepadAxes[i] = state.axes[i];
            }
        }
    } else {
        s_gamepadButtons.fill(false);
        s_gamepadAxes.fill(0.0f);
    }
}

// Keyboard
bool Input::isKeyPressed(KeyCode key) {
    int keyCode = static_cast<int>(key);
    if (keyCode < 0 || keyCode >= static_cast<int>(KeyCode::MaxKeys)) {
        return false;
    }
    return s_keys[keyCode];
}

bool Input::isKeyDown(KeyCode key) {
    int keyCode = static_cast<int>(key);
    if (keyCode < 0 || keyCode >= static_cast<int>(KeyCode::MaxKeys)) {
        return false;
    }
    return s_keys[keyCode] && !s_keysPrevious[keyCode];
}

bool Input::isKeyReleased(KeyCode key) {
    int keyCode = static_cast<int>(key);
    if (keyCode < 0 || keyCode >= static_cast<int>(KeyCode::MaxKeys)) {
        return false;
    }
    return !s_keys[keyCode] && s_keysPrevious[keyCode];
}

// Mouse buttons
bool Input::isMouseButtonPressed(MouseButton button) {
    int buttonCode = static_cast<int>(button);
    if (buttonCode < 0 || buttonCode >= static_cast<int>(MouseButton::MaxButtons)) {
        return false;
    }
    return s_mouseButtons[buttonCode];
}

bool Input::isMouseButtonDown(MouseButton button) {
    int buttonCode = static_cast<int>(button);
    if (buttonCode < 0 || buttonCode >= static_cast<int>(MouseButton::MaxButtons)) {
        return false;
    }
    return s_mouseButtons[buttonCode] && !s_mouseButtonsPrevious[buttonCode];
}

bool Input::isMouseButtonReleased(MouseButton button) {
    int buttonCode = static_cast<int>(button);
    if (buttonCode < 0 || buttonCode >= static_cast<int>(MouseButton::MaxButtons)) {
        return false;
    }
    return !s_mouseButtons[buttonCode] && s_mouseButtonsPrevious[buttonCode];
}

// Mouse position
glm::vec2 Input::getMousePosition() {
    return s_mousePosition;
}

glm::vec2 Input::getMouseDelta() {
    return s_mousePosition - s_mousePositionPrevious;
}

// Mouse scroll
glm::vec2 Input::getMouseScroll() {
    return s_mouseScroll;
}

// Gamepad
bool Input::isGamepadConnected() {
    return glfwJoystickPresent(GLFW_JOYSTICK_1) && glfwJoystickIsGamepad(GLFW_JOYSTICK_1);
}

bool Input::isGamepadButtonPressed(GamepadButton button) {
    int buttonCode = static_cast<int>(button);
    if (buttonCode < 0 || buttonCode >= static_cast<int>(GamepadButton::MaxButtons)) {
        return false;
    }
    return s_gamepadButtons[buttonCode];
}

bool Input::isGamepadButtonDown(GamepadButton button) {
    int buttonCode = static_cast<int>(button);
    if (buttonCode < 0 || buttonCode >= static_cast<int>(GamepadButton::MaxButtons)) {
        return false;
    }
    return s_gamepadButtons[buttonCode] && !s_gamepadButtonsPrevious[buttonCode];
}

bool Input::isGamepadButtonReleased(GamepadButton button) {
    int buttonCode = static_cast<int>(button);
    if (buttonCode < 0 || buttonCode >= static_cast<int>(GamepadButton::MaxButtons)) {
        return false;
    }
    return !s_gamepadButtons[buttonCode] && s_gamepadButtonsPrevious[buttonCode];
}

float Input::getGamepadAxis(GamepadAxis axis) {
    int axisCode = static_cast<int>(axis);
    if (axisCode < 0 || axisCode >= static_cast<int>(GamepadAxis::MaxAxes)) {
        return 0.0f;
    }
    return s_gamepadAxes[axisCode];
}

glm::vec2 Input::getGamepadLeftStick() {
    return glm::vec2(
        getGamepadAxis(GamepadAxis::LeftX),
        getGamepadAxis(GamepadAxis::LeftY)
    );
}

glm::vec2 Input::getGamepadRightStick() {
    return glm::vec2(
        getGamepadAxis(GamepadAxis::RightX),
        getGamepadAxis(GamepadAxis::RightY)
    );
}

// Modifiers
bool Input::isShiftPressed() {
    return isKeyPressed(KeyCode::LeftShift) || isKeyPressed(KeyCode::RightShift);
}

bool Input::isControlPressed() {
    return isKeyPressed(KeyCode::LeftControl) || isKeyPressed(KeyCode::RightControl);
}

bool Input::isAltPressed() {
    return isKeyPressed(KeyCode::LeftAlt) || isKeyPressed(KeyCode::RightAlt);
}

bool Input::isSuperPressed() {
    return isKeyPressed(KeyCode::LeftSuper) || isKeyPressed(KeyCode::RightSuper);
}

// GLFW callbacks
void Input::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key >= 0 && key < static_cast<int>(KeyCode::MaxKeys)) {
        if (action == GLFW_PRESS) {
            s_keys[key] = true;
#ifndef NDEBUG
            auto enumName = magic_enum::enum_name(static_cast<KeyCode>(key));
            std::println("[Input Debug] Key pressed: {} (code: {})", enumName, key);
#endif
        } else if (action == GLFW_RELEASE) {
            s_keys[key] = false;
#ifndef NDEBUG
            auto enumName = magic_enum::enum_name(static_cast<KeyCode>(key));
            std::println("[Input Debug] Key released: {} (code: {})", enumName, key);
#endif
        }
    }
}

void Input::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button >= 0 && button < static_cast<int>(MouseButton::MaxButtons)) {
        if (action == GLFW_PRESS) {
            s_mouseButtons[button] = true;
        } else if (action == GLFW_RELEASE) {
            s_mouseButtons[button] = false;
        }
    }
}

void Input::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    s_mouseScroll = glm::vec2(static_cast<float>(xoffset), static_cast<float>(yoffset));
}

} // namespace VoxelEngine