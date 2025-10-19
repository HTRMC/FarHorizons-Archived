#pragma once

#include <string>
#include <functional>

namespace VoxelEngine {

// Event types
enum class EventType {
    None = 0,
    // Window events
    WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,
    // Key events
    KeyPressed, KeyReleased, KeyTyped,
    // Mouse events
    MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled,
    // Gamepad events
    GamepadConnected, GamepadDisconnected, GamepadButtonPressed, GamepadButtonReleased
};

// Event categories for filtering
enum EventCategory {
    None = 0,
    EventCategoryApplication = 1 << 0,
    EventCategoryInput = 1 << 1,
    EventCategoryKeyboard = 1 << 2,
    EventCategoryMouse = 1 << 3,
    EventCategoryMouseButton = 1 << 4,
    EventCategoryGamepad = 1 << 5
};

// Macros for implementing event type and category methods
#define EVENT_CLASS_TYPE(type) \
    static EventType getStaticType() { return EventType::type; } \
    virtual EventType getEventType() const override { return getStaticType(); } \
    virtual const char* getName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category) \
    virtual int getCategoryFlags() const override { return category; }

// Base Event class
class Event {
public:
    virtual ~Event() = default;

    bool handled = false;

    virtual EventType getEventType() const = 0;
    virtual const char* getName() const = 0;
    virtual int getCategoryFlags() const = 0;
    virtual std::string toString() const { return getName(); }

    bool isInCategory(EventCategory category) const {
        return getCategoryFlags() & category;
    }
};

// Event dispatcher
class EventDispatcher {
public:
    EventDispatcher(Event& event) : m_event(event) {}

    template<typename T, typename F>
    bool dispatch(const F& func) {
        if (m_event.getEventType() == T::getStaticType()) {
            m_event.handled |= func(static_cast<T&>(m_event));
            return true;
        }
        return false;
    }

private:
    Event& m_event;
};

// Window Events
class WindowResizeEvent : public Event {
public:
    WindowResizeEvent(unsigned int width, unsigned int height)
        : m_width(width), m_height(height) {}

    unsigned int getWidth() const { return m_width; }
    unsigned int getHeight() const { return m_height; }

    std::string toString() const override {
        return std::string("WindowResizeEvent: ") + std::to_string(m_width) + ", " + std::to_string(m_height);
    }

    EVENT_CLASS_TYPE(WindowResize)
    EVENT_CLASS_CATEGORY(EventCategoryApplication)

private:
    unsigned int m_width, m_height;
};

class WindowCloseEvent : public Event {
public:
    WindowCloseEvent() = default;

    EVENT_CLASS_TYPE(WindowClose)
    EVENT_CLASS_CATEGORY(EventCategoryApplication)
};

class WindowFocusEvent : public Event {
public:
    WindowFocusEvent(bool focused) : m_focused(focused) {}

    bool isFocused() const { return m_focused; }

    EVENT_CLASS_TYPE(WindowFocus)
    EVENT_CLASS_CATEGORY(EventCategoryApplication)

private:
    bool m_focused;
};

// Key Events
class KeyEvent : public Event {
public:
    int getKeyCode() const { return m_keyCode; }

    EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)

protected:
    KeyEvent(int keycode) : m_keyCode(keycode) {}

    int m_keyCode;
};

class KeyPressedEvent : public KeyEvent {
public:
    KeyPressedEvent(int keycode, bool isRepeat = false)
        : KeyEvent(keycode), m_isRepeat(isRepeat) {}

    bool isRepeat() const { return m_isRepeat; }

    std::string toString() const override {
        return std::string("KeyPressedEvent: ") + std::to_string(m_keyCode) +
               (m_isRepeat ? " (repeat)" : "");
    }

    EVENT_CLASS_TYPE(KeyPressed)

private:
    bool m_isRepeat;
};

class KeyReleasedEvent : public KeyEvent {
public:
    KeyReleasedEvent(int keycode) : KeyEvent(keycode) {}

    std::string toString() const override {
        return std::string("KeyReleasedEvent: ") + std::to_string(m_keyCode);
    }

    EVENT_CLASS_TYPE(KeyReleased)
};

class KeyTypedEvent : public KeyEvent {
public:
    KeyTypedEvent(int keycode) : KeyEvent(keycode) {}

    std::string toString() const override {
        return std::string("KeyTypedEvent: ") + std::to_string(m_keyCode);
    }

    EVENT_CLASS_TYPE(KeyTyped)
};

// Mouse Events
class MouseMovedEvent : public Event {
public:
    MouseMovedEvent(float x, float y) : m_mouseX(x), m_mouseY(y) {}

    float getX() const { return m_mouseX; }
    float getY() const { return m_mouseY; }

    std::string toString() const override {
        return std::string("MouseMovedEvent: ") + std::to_string(m_mouseX) + ", " + std::to_string(m_mouseY);
    }

    EVENT_CLASS_TYPE(MouseMoved)
    EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

private:
    float m_mouseX, m_mouseY;
};

class MouseScrolledEvent : public Event {
public:
    MouseScrolledEvent(float xOffset, float yOffset)
        : m_xOffset(xOffset), m_yOffset(yOffset) {}

    float getXOffset() const { return m_xOffset; }
    float getYOffset() const { return m_yOffset; }

    std::string toString() const override {
        return std::string("MouseScrolledEvent: ") + std::to_string(m_xOffset) + ", " + std::to_string(m_yOffset);
    }

    EVENT_CLASS_TYPE(MouseScrolled)
    EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

private:
    float m_xOffset, m_yOffset;
};

class MouseButtonEvent : public Event {
public:
    int getMouseButton() const { return m_button; }

    EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput | EventCategoryMouseButton)

protected:
    MouseButtonEvent(int button) : m_button(button) {}

    int m_button;
};

class MouseButtonPressedEvent : public MouseButtonEvent {
public:
    MouseButtonPressedEvent(int button) : MouseButtonEvent(button) {}

    std::string toString() const override {
        return std::string("MouseButtonPressedEvent: ") + std::to_string(m_button);
    }

    EVENT_CLASS_TYPE(MouseButtonPressed)
};

class MouseButtonReleasedEvent : public MouseButtonEvent {
public:
    MouseButtonReleasedEvent(int button) : MouseButtonEvent(button) {}

    std::string toString() const override {
        return std::string("MouseButtonReleasedEvent: ") + std::to_string(m_button);
    }

    EVENT_CLASS_TYPE(MouseButtonReleased)
};

// Gamepad Events
class GamepadConnectedEvent : public Event {
public:
    GamepadConnectedEvent(int joystickID) : m_joystickID(joystickID) {}

    int getJoystickID() const { return m_joystickID; }

    EVENT_CLASS_TYPE(GamepadConnected)
    EVENT_CLASS_CATEGORY(EventCategoryGamepad | EventCategoryInput)

private:
    int m_joystickID;
};

class GamepadDisconnectedEvent : public Event {
public:
    GamepadDisconnectedEvent(int joystickID) : m_joystickID(joystickID) {}

    int getJoystickID() const { return m_joystickID; }

    EVENT_CLASS_TYPE(GamepadDisconnected)
    EVENT_CLASS_CATEGORY(EventCategoryGamepad | EventCategoryInput)

private:
    int m_joystickID;
};

} // namespace VoxelEngine