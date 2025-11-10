#include "InputAction.hpp"
#include "InputSystem.hpp"

namespace FarHorizon {

std::unordered_map<std::string, InputAction> InputActionManager::s_actions;
std::unordered_map<std::string, InputAxis> InputActionManager::s_axes;

InputAction& InputActionManager::createAction(const std::string& name) {
    auto it = s_actions.find(name);
    if (it != s_actions.end()) {
        return it->second;
    }
    return s_actions.emplace(name, InputAction(name)).first->second;
}

InputAxis& InputActionManager::createAxis(const std::string& name) {
    auto it = s_axes.find(name);
    if (it != s_axes.end()) {
        return it->second;
    }
    return s_axes.emplace(name, InputAxis(name)).first->second;
}

void InputActionManager::processInput() {
    // Process actions (discrete button presses)
    for (auto& [name, action] : s_actions) {
        bool triggered = false;

        for (const auto& binding : action.getBindings()) {
            switch (binding.type) {
                case InputBindingType::Key: {
                    auto key = std::get<KeyCode>(binding.input);
                    if (InputSystem::isKeyDown(key)) {
                        triggered = true;
                    }
                    break;
                }
                case InputBindingType::MouseButton: {
                    auto button = std::get<MouseButton>(binding.input);
                    if (InputSystem::isMouseButtonDown(button)) {
                        triggered = true;
                    }
                    break;
                }
                case InputBindingType::GamepadButton: {
                    auto button = std::get<GamepadButton>(binding.input);
                    if (InputSystem::isGamepadButtonDown(button)) {
                        triggered = true;
                    }
                    break;
                }
                default:
                    break;
            }

            if (triggered) {
                action.trigger();
                break; // One binding is enough to trigger action
            }
        }
    }

    // Process axes (continuous values)
    for (auto& [name, axis] : s_axes) {
        float value = 0.0f;

        for (const auto& binding : axis.getBindings()) {
            switch (binding.type) {
                case InputBindingType::Key: {
                    auto key = std::get<KeyCode>(binding.input);
                    if (InputSystem::isKeyPressed(key)) {
                        value += binding.scale;
                    }
                    break;
                }
                case InputBindingType::MouseButton: {
                    auto button = std::get<MouseButton>(binding.input);
                    if (InputSystem::isMouseButtonPressed(button)) {
                        value += binding.scale;
                    }
                    break;
                }
                case InputBindingType::GamepadButton: {
                    auto button = std::get<GamepadButton>(binding.input);
                    if (InputSystem::isGamepadButtonPressed(button)) {
                        value += binding.scale;
                    }
                    break;
                }
                case InputBindingType::GamepadAxis: {
                    auto axisType = std::get<GamepadAxis>(binding.input);
                    float axisValue = InputSystem::getGamepadAxis(axisType);
                    value += axisValue * binding.scale;
                    break;
                }
            }
        }

        // Clamp to [-1, 1]
        value = std::max(-1.0f, std::min(1.0f, value));

        // Only trigger if value is non-zero
        if (std::abs(value) > 0.0001f) {
            axis.trigger(value);
        }
    }
}

void InputActionManager::clear() {
    s_actions.clear();
    s_axes.clear();
}

} // namespace FarHorizon