#pragma once

#include <GLFW/glfw3.h>
#include <magic_enum/magic_enum.hpp>

namespace VoxelEngine {

// Key codes (matching GLFW)
enum class KeyCode {
    // Printable keys
    Space = GLFW_KEY_SPACE,
    Apostrophe = GLFW_KEY_APOSTROPHE,
    Comma = GLFW_KEY_COMMA,
    Minus = GLFW_KEY_MINUS,
    Period = GLFW_KEY_PERIOD,
    Slash = GLFW_KEY_SLASH,

    // Number keys (prefixed with D for "Digit" since identifiers can't start with numbers)
    Zero = GLFW_KEY_0,
    One = GLFW_KEY_1,
    Two = GLFW_KEY_2,
    Three = GLFW_KEY_3,
    Four = GLFW_KEY_4,
    Five = GLFW_KEY_5,
    Six = GLFW_KEY_6,
    Seven = GLFW_KEY_7,
    Eight = GLFW_KEY_8,
    Nine = GLFW_KEY_9,

    Semicolon = GLFW_KEY_SEMICOLON,
    Equal = GLFW_KEY_EQUAL,

    A = GLFW_KEY_A, B = GLFW_KEY_B, C = GLFW_KEY_C, D = GLFW_KEY_D,
    E = GLFW_KEY_E, F = GLFW_KEY_F, G = GLFW_KEY_G, H = GLFW_KEY_H,
    I = GLFW_KEY_I, J = GLFW_KEY_J, K = GLFW_KEY_K, L = GLFW_KEY_L,
    M = GLFW_KEY_M, N = GLFW_KEY_N, O = GLFW_KEY_O, P = GLFW_KEY_P,
    Q = GLFW_KEY_Q, R = GLFW_KEY_R, S = GLFW_KEY_S, T = GLFW_KEY_T,
    U = GLFW_KEY_U, V = GLFW_KEY_V, W = GLFW_KEY_W, X = GLFW_KEY_X,
    Y = GLFW_KEY_Y, Z = GLFW_KEY_Z,

    LeftBracket = GLFW_KEY_LEFT_BRACKET,
    Backslash = GLFW_KEY_BACKSLASH,
    RightBracket = GLFW_KEY_RIGHT_BRACKET,
    GraveAccent = GLFW_KEY_GRAVE_ACCENT,

    // Function keys
    Escape = GLFW_KEY_ESCAPE,
    Enter = GLFW_KEY_ENTER,
    Tab = GLFW_KEY_TAB,
    Backspace = GLFW_KEY_BACKSPACE,
    Insert = GLFW_KEY_INSERT,
    Delete = GLFW_KEY_DELETE,
    Right = GLFW_KEY_RIGHT,
    Left = GLFW_KEY_LEFT,
    Down = GLFW_KEY_DOWN,
    Up = GLFW_KEY_UP,
    PageUp = GLFW_KEY_PAGE_UP,
    PageDown = GLFW_KEY_PAGE_DOWN,
    Home = GLFW_KEY_HOME,
    End = GLFW_KEY_END,
    CapsLock = GLFW_KEY_CAPS_LOCK,
    ScrollLock = GLFW_KEY_SCROLL_LOCK,
    NumLock = GLFW_KEY_NUM_LOCK,
    PrintScreen = GLFW_KEY_PRINT_SCREEN,
    Pause = GLFW_KEY_PAUSE,

    F1 = GLFW_KEY_F1, F2 = GLFW_KEY_F2, F3 = GLFW_KEY_F3, F4 = GLFW_KEY_F4,
    F5 = GLFW_KEY_F5, F6 = GLFW_KEY_F6, F7 = GLFW_KEY_F7, F8 = GLFW_KEY_F8,
    F9 = GLFW_KEY_F9, F10 = GLFW_KEY_F10, F11 = GLFW_KEY_F11, F12 = GLFW_KEY_F12,

    // Keypad
    KPZero = GLFW_KEY_KP_0, KPOne = GLFW_KEY_KP_1, KPTwo = GLFW_KEY_KP_2,
    KPThree = GLFW_KEY_KP_3, KPFour = GLFW_KEY_KP_4, KPFive = GLFW_KEY_KP_5,
    KPSix = GLFW_KEY_KP_6, KPSeven = GLFW_KEY_KP_7, KPEight = GLFW_KEY_KP_8, KPNine = GLFW_KEY_KP_9,
    KPDecimal = GLFW_KEY_KP_DECIMAL,
    KPDivide = GLFW_KEY_KP_DIVIDE,
    KPMultiply = GLFW_KEY_KP_MULTIPLY,
    KPSubtract = GLFW_KEY_KP_SUBTRACT,
    KPAdd = GLFW_KEY_KP_ADD,
    KPEnter = GLFW_KEY_KP_ENTER,
    KPEqual = GLFW_KEY_KP_EQUAL,

    // Modifiers
    LeftShift = GLFW_KEY_LEFT_SHIFT,
    LeftControl = GLFW_KEY_LEFT_CONTROL,
    LeftAlt = GLFW_KEY_LEFT_ALT,
    LeftSuper = GLFW_KEY_LEFT_SUPER,
    RightShift = GLFW_KEY_RIGHT_SHIFT,
    RightControl = GLFW_KEY_RIGHT_CONTROL,
    RightAlt = GLFW_KEY_RIGHT_ALT,
    RightSuper = GLFW_KEY_RIGHT_SUPER,
    Menu = GLFW_KEY_MENU,

    MaxKeys = GLFW_KEY_LAST + 1,

    Unknown = GLFW_KEY_UNKNOWN
};

enum class MouseButton {
    Left = GLFW_MOUSE_BUTTON_LEFT,
    Right = GLFW_MOUSE_BUTTON_RIGHT,
    Middle = GLFW_MOUSE_BUTTON_MIDDLE,
    Button4 = GLFW_MOUSE_BUTTON_4,
    Button5 = GLFW_MOUSE_BUTTON_5,
    Button6 = GLFW_MOUSE_BUTTON_6,
    Button7 = GLFW_MOUSE_BUTTON_7,
    Button8 = GLFW_MOUSE_BUTTON_8,

    MaxButtons = GLFW_MOUSE_BUTTON_LAST + 1
};

enum class GamepadButton {
    A = GLFW_GAMEPAD_BUTTON_A,
    B = GLFW_GAMEPAD_BUTTON_B,
    X = GLFW_GAMEPAD_BUTTON_X,
    Y = GLFW_GAMEPAD_BUTTON_Y,
    LeftBumper = GLFW_GAMEPAD_BUTTON_LEFT_BUMPER,
    RightBumper = GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER,
    Back = GLFW_GAMEPAD_BUTTON_BACK,
    Start = GLFW_GAMEPAD_BUTTON_START,
    Guide = GLFW_GAMEPAD_BUTTON_GUIDE,
    LeftThumb = GLFW_GAMEPAD_BUTTON_LEFT_THUMB,
    RightThumb = GLFW_GAMEPAD_BUTTON_RIGHT_THUMB,
    DpadUp = GLFW_GAMEPAD_BUTTON_DPAD_UP,
    DpadRight = GLFW_GAMEPAD_BUTTON_DPAD_RIGHT,
    DpadDown = GLFW_GAMEPAD_BUTTON_DPAD_DOWN,
    DpadLeft = GLFW_GAMEPAD_BUTTON_DPAD_LEFT,

    MaxButtons = GLFW_GAMEPAD_BUTTON_LAST + 1
};

enum class GamepadAxis {
    LeftX = GLFW_GAMEPAD_AXIS_LEFT_X,
    LeftY = GLFW_GAMEPAD_AXIS_LEFT_Y,
    RightX = GLFW_GAMEPAD_AXIS_RIGHT_X,
    RightY = GLFW_GAMEPAD_AXIS_RIGHT_Y,
    LeftTrigger = GLFW_GAMEPAD_AXIS_LEFT_TRIGGER,
    RightTrigger = GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER,

    MaxAxes = GLFW_GAMEPAD_AXIS_LAST + 1
};

} // namespace VoxelEngine

// Configure magic_enum to support GLFW key codes (range 32-348)
template <>
struct magic_enum::customize::enum_range<VoxelEngine::KeyCode> {
    static constexpr int min = -1;  // GLFW_KEY_UNKNOWN is -1
    static constexpr int max = 400; // GLFW_KEY_LAST is 348, give some headroom
};
