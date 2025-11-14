#include "Camera.hpp"
#include "InputSystem.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <iostream>

namespace FarHorizon {

void Camera::init(const glm::vec3& position, float aspectRatio, float fov) {
    position_ = position;
    aspectRatio_ = aspectRatio;
    fov_ = fov;

    updateVectors();
    updateViewMatrix();
    updateProjectionMatrix();
}

void Camera::setKeybinds(const std::unordered_map<std::string, std::string>& keybinds) {
    // Parse keybind strings to KeyCode enums once (using enum for fast lookup)
    auto getKey = [&](KeybindAction action, KeyCode defaultKey) -> KeyCode {
        auto it = keybinds.find(keybindActionToString(action));
        if (it != keybinds.end()) {
            KeyCode parsed = InputSystem::stringToKeyCode(it->second);
            return (parsed != KeyCode::Unknown) ? parsed : defaultKey;
        }
        return defaultKey;
    };

    keyForward_ = getKey(KeybindAction::Forward, KeyCode::W);
    keyBack_ = getKey(KeybindAction::Back, KeyCode::S);
    keyLeft_ = getKey(KeybindAction::Left, KeyCode::A);
    keyRight_ = getKey(KeybindAction::Right, KeyCode::D);
    keyJump_ = getKey(KeybindAction::Jump, KeyCode::Space);
    keySneak_ = getKey(KeybindAction::Sneak, KeyCode::LeftShift);
}

void Camera::update(float deltaTime) {
    // Handle movement using configured keybinds
    glm::vec3 moveDirection(0.0f);

    if (InputSystem::isKeyPressed(keyForward_)) {
        moveDirection += forward_;
    }
    if (InputSystem::isKeyPressed(keyBack_)) {
        moveDirection -= forward_;
    }
    if (InputSystem::isKeyPressed(keyLeft_)) {
        moveDirection -= right_;
    }
    if (InputSystem::isKeyPressed(keyRight_)) {
        moveDirection += right_;
    }
    if (InputSystem::isKeyPressed(keyJump_)) {
        moveDirection += glm::vec3(0.0f, 1.0f, 0.0f); // World up
    }
    if (InputSystem::isKeyPressed(keySneak_)) {
        moveDirection -= glm::vec3(0.0f, 1.0f, 0.0f); // World down
    }

    // Normalize and apply speed
    if (glm::length(moveDirection) > 0.0f) {
        move(glm::normalize(moveDirection), deltaTime);
    }

    // Handle mouse rotation
    // Use MouseCapture if available (respects cursor lock state), otherwise fall back to InputSystem
    glm::vec2 mouseDelta(0.0f);

    if (mouseCapture_ && mouseCapture_->isCursorLocked()) {
        // Use mouse capture deltas (only when cursor is locked)
        mouseDelta.x = static_cast<float>(mouseCapture_->getCursorDeltaX());
        mouseDelta.y = static_cast<float>(mouseCapture_->getCursorDeltaY());
    } else if (!mouseCapture_) {
        // Fall back to InputSystem for backward compatibility
        mouseDelta = InputSystem::getMouseDelta();
    }
    // If cursor is unlocked, mouseDelta remains (0, 0) - no camera rotation

    if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
        // Apply Minecraft's mouse sensitivity formula
        // sensitivity ranges from 0.0 to 1.0 (our slider is 1-100%, converted to 0.01-1.0)
        float d = mouseSensitivity_ * 0.6f + 0.2f;  // Map to 0.2-0.8 range
        float e = d * d * d;  // Cube for non-linear response
        float f = e * 8.0f;   // Final multiplier

        // Apply multiplier and final scaling (0.15 from Minecraft's Entity.changeLookDirection)
        float yawDelta = mouseDelta.x * f * 0.15f;
        float pitchDelta = -mouseDelta.y * f * 0.15f; // Inverted Y
        rotate(yawDelta, pitchDelta);
    }

    // Reset mouse capture deltas after processing
    if (mouseCapture_) {
        mouseCapture_->resetDeltas();
    }
}

void Camera::move(const glm::vec3& direction, float deltaTime) {
    position_ += direction * moveSpeed_ * deltaTime;
    updateViewMatrix();
}

void Camera::rotate(float yaw, float pitch) {
    yaw_ += yaw;
    pitch_ += pitch;

    // Constrain pitch to avoid gimbal lock
    pitch_ = std::clamp(pitch_, -89.0f, 89.0f);

    updateVectors();
    updateViewMatrix();
}

void Camera::updateViewMatrix() {
    viewMatrix_ = glm::lookAt(position_, position_ + forward_, up_);
}

void Camera::updateProjectionMatrix() {
    // Vulkan uses inverted Y clip space compared to OpenGL
    projectionMatrix_ = glm::perspective(glm::radians(fov_), aspectRatio_, nearPlane_, farPlane_);
    projectionMatrix_[1][1] *= -1; // Flip Y for Vulkan
}

void Camera::updateVectors() {
    // Calculate forward vector from yaw and pitch
    // Using Minecraft's coordinate system:
    // Yaw 0° = +Z (south), 90° = -X (west), 180° = -Z (north), 270° = +X (east)
    glm::vec3 forward;
    forward.x = -sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    forward.y = sin(glm::radians(pitch_));
    forward.z = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    forward_ = glm::normalize(forward);

    // Calculate right and up vectors
    right_ = glm::normalize(glm::cross(forward_, glm::vec3(0.0f, 1.0f, 0.0f)));
    up_ = glm::normalize(glm::cross(right_, forward_));
}

} // namespace FarHorizon