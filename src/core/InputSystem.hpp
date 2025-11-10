#pragma once

#include "InputTypes.hpp"
#include "InputEvent.hpp"
#include "InputQueue.hpp"
#include "MouseCapture.hpp"
#include <GLFW/glfw3.h>
#include <array>
#include <glm/glm.hpp>
#include <functional>
#include <unordered_map>
#include <string>

namespace FarHorizon {

// Event listener callback type
using InputEventCallback = std::function<void(const InputEvent&)>;

// input system with event queue
class InputSystem {
public:
    static void init(GLFWwindow* window);
    static void shutdown();

    // Process all queued events (call once per frame on game thread)
    static void processEvents();

    // Subscribe to specific event types
    static uint32_t addEventListener(InputEventType type, InputEventCallback callback);
    static void removeEventListener(uint32_t listenerID);

    // Keyboard state queries (processed from events)
    static bool isKeyPressed(KeyCode key);
    static bool isKeyDown(KeyCode key);      // Just pressed this frame
    static bool isKeyReleased(KeyCode key);  // Just released this frame

    // Mouse button queries
    static bool isMouseButtonPressed(MouseButton button);
    static bool isMouseButtonDown(MouseButton button);
    static bool isMouseButtonReleased(MouseButton button);

    // Mouse state
    static glm::vec2 getMousePosition();
    static glm::vec2 getMouseDelta();
    static glm::vec2 getMouseScroll();

    // Gamepad queries
    static bool isGamepadConnected(int joystickID = 0);
    static bool isGamepadButtonPressed(GamepadButton button, int joystickID = 0);
    static bool isGamepadButtonDown(GamepadButton button, int joystickID = 0);
    static bool isGamepadButtonReleased(GamepadButton button, int joystickID = 0);
    static float getGamepadAxis(GamepadAxis axis, int joystickID = 0);
    static glm::vec2 getGamepadLeftStick(int joystickID = 0);
    static glm::vec2 getGamepadRightStick(int joystickID = 0);

    // Modifiers
    static bool isShiftPressed();
    static bool isControlPressed();
    static bool isAltPressed();
    static bool isSuperPressed();

    // Configuration
    static void setAnalogDeadzone(float deadzone) { s_analogDeadzone = deadzone; }
    static float getAnalogDeadzone() { return s_analogDeadzone; }

    // Mouse capture integration
    static void setMouseCapture(MouseCapture* mouseCapture) { s_mouseCapture = mouseCapture; }

    // Convert keybind string to KeyCode (e.g., "key.keyboard.w" -> KeyCode::W)
    static KeyCode stringToKeyCode(const std::string& keybind);
    static MouseButton stringToMouseButton(const std::string& keybind);

    // Get raw event queue for advanced users
    static const std::vector<InputEvent>& getProcessedEvents() { return s_processedEvents; }

private:
    // GLFW callbacks (run on input thread)
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void joystickCallback(int jid, int event);

    // Process individual events
    static void processKeyEvent(const KeyEventData& event);
    static void processMouseButtonEvent(const MouseButtonEventData& event);
    static void processMouseMovedEvent(const MouseMovedEventData& event);
    static void processMouseScrollEvent(const MouseScrollEventData& event);
    static void processGamepadButtonEvent(const GamepadButtonEventData& event);
    static void processGamepadAxisEvent(const GamepadAxisEventData& event);

    // Notify event listeners
    static void notifyListeners(const InputEvent& event);

    // Apply deadzone to analog input
    static float applyDeadzone(float value);
    static glm::vec2 applyDeadzone(glm::vec2 value);

private:
    static GLFWwindow* s_window;
    static InputQueue s_eventQueue;
    static std::vector<InputEvent> s_processedEvents;

    // Event listeners
    static uint32_t s_nextListenerID;
    static std::unordered_map<uint32_t, std::pair<InputEventType, InputEventCallback>> s_listeners;

    // Keyboard state
    static std::array<bool, static_cast<size_t>(KeyCode::MaxKeys)> s_keys;
    static std::array<bool, static_cast<size_t>(KeyCode::MaxKeys)> s_keysPrevious;

    // Mouse state
    static std::array<bool, static_cast<size_t>(MouseButton::MaxButtons)> s_mouseButtons;
    static std::array<bool, static_cast<size_t>(MouseButton::MaxButtons)> s_mouseButtonsPrevious;
    static glm::vec2 s_mousePosition;
    static glm::vec2 s_mousePositionPrevious;
    static glm::vec2 s_mouseDelta;
    static glm::vec2 s_mouseScroll;

    // Gamepad state (supporting multiple gamepads)
    struct GamepadState {
        bool connected = false;
        std::array<bool, static_cast<size_t>(GamepadButton::MaxButtons)> buttons = {};
        std::array<bool, static_cast<size_t>(GamepadButton::MaxButtons)> buttonsPrevious = {};
        std::array<float, static_cast<size_t>(GamepadAxis::MaxAxes)> axes = {};
        std::array<float, static_cast<size_t>(GamepadAxis::MaxAxes)> axesPrevious = {};
    };
    static std::array<GamepadState, 4> s_gamepads; // Support up to 4 gamepads

    // Configuration
    static float s_analogDeadzone;

    // Mouse capture system
    static MouseCapture* s_mouseCapture;
};

} // namespace FarHorizon