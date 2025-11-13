#pragma once

#include "InputTypes.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <variant>

namespace FarHorizon {

// Input binding types
enum class InputBindingType {
    Key,
    MouseButton,
    GamepadButton,
    GamepadAxis
};

// Input binding for keys/buttons
struct InputBinding {
    InputBindingType type;
    std::variant<KeyCode, MouseButton, GamepadButton, GamepadAxis> input;
    float scale = 1.0f; // For axes (positive or negative)

    // Key constructors
    InputBinding(KeyCode key) : type(InputBindingType::Key), input(key), scale(1.0f) {}
    InputBinding(KeyCode key, float keyScale) : type(InputBindingType::Key), input(key), scale(keyScale) {}

    // Mouse button constructors
    InputBinding(MouseButton button) : type(InputBindingType::MouseButton), input(button), scale(1.0f) {}
    InputBinding(MouseButton button, float buttonScale) : type(InputBindingType::MouseButton), input(button), scale(buttonScale) {}

    // Gamepad button constructors
    InputBinding(GamepadButton button) : type(InputBindingType::GamepadButton), input(button), scale(1.0f) {}
    InputBinding(GamepadButton button, float buttonScale) : type(InputBindingType::GamepadButton), input(button), scale(buttonScale) {}

    // Gamepad axis constructor
    InputBinding(GamepadAxis axis, float axisScale = 1.0f)
        : type(InputBindingType::GamepadAxis), input(axis), scale(axisScale) {}
};

// Action callback types
using ActionCallback = std::function<void()>;
using AxisCallback = std::function<void(float)>;

// Input action (discrete button press)
class InputAction {
public:
    InputAction(const std::string& name) : name_(name) {}

    void addBinding(const InputBinding& binding) {
        bindings_.push_back(binding);
    }

    void bind(ActionCallback callback) {
        callback_ = callback;
    }

    void trigger() {
        if (callback_) callback_();
    }

    const std::string& getName() const { return name_; }
    const std::vector<InputBinding>& getBindings() const { return bindings_; }

private:
    std::string name_;
    std::vector<InputBinding> bindings_;
    ActionCallback callback_;
};

// Input axis (continuous value like movement or look)
class InputAxis {
public:
    InputAxis(const std::string& name) : name_(name) {}

    void addBinding(const InputBinding& binding) {
        bindings_.push_back(binding);
    }

    void bind(AxisCallback callback) {
        callback_ = callback;
    }

    void trigger(float value) {
        if (callback_) callback_(value);
    }

    const std::string& getName() const { return name_; }
    const std::vector<InputBinding>& getBindings() const { return bindings_; }

private:
    std::string name_;
    std::vector<InputBinding> bindings_;
    AxisCallback callback_;
};

// Input action manager (like Unreal's Input Component)
class InputActionManager {
public:
    // Create or get an action
    static InputAction& createAction(const std::string& name);
    static InputAxis& createAxis(const std::string& name);

    // Process input (call each frame after InputSystem::processEvents())
    static void processInput();

    // Clear all actions and axes
    static void clear();

private:
    static std::unordered_map<std::string, InputAction> s_actions;
    static std::unordered_map<std::string, InputAxis> s_axes;
};

} // namespace FarHorizon