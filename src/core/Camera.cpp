#include "Camera.hpp"
#include "InputSystem.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <iostream>

namespace VoxelEngine {

void Camera::init(const glm::vec3& position, float aspectRatio, float fov) {
    m_position = position;
    m_aspectRatio = aspectRatio;
    m_fov = fov;

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

    m_keyForward = getKey(KeybindAction::Forward, KeyCode::W);
    m_keyBack = getKey(KeybindAction::Back, KeyCode::S);
    m_keyLeft = getKey(KeybindAction::Left, KeyCode::A);
    m_keyRight = getKey(KeybindAction::Right, KeyCode::D);
    m_keyJump = getKey(KeybindAction::Jump, KeyCode::Space);
    m_keySneak = getKey(KeybindAction::Sneak, KeyCode::LeftShift);
}

void Camera::update(float deltaTime) {
    // Handle movement using configured keybinds
    glm::vec3 moveDirection(0.0f);

    if (InputSystem::isKeyPressed(m_keyForward)) {
        moveDirection += m_forward;
    }
    if (InputSystem::isKeyPressed(m_keyBack)) {
        moveDirection -= m_forward;
    }
    if (InputSystem::isKeyPressed(m_keyLeft)) {
        moveDirection -= m_right;
    }
    if (InputSystem::isKeyPressed(m_keyRight)) {
        moveDirection += m_right;
    }
    if (InputSystem::isKeyPressed(m_keyJump)) {
        moveDirection += glm::vec3(0.0f, 1.0f, 0.0f); // World up
    }
    if (InputSystem::isKeyPressed(m_keySneak)) {
        moveDirection -= glm::vec3(0.0f, 1.0f, 0.0f); // World down
    }

    // Normalize and apply speed
    if (glm::length(moveDirection) > 0.0f) {
        move(glm::normalize(moveDirection), deltaTime);
    }

    // Handle mouse rotation
    // Use MouseCapture if available (respects cursor lock state), otherwise fall back to InputSystem
    glm::vec2 mouseDelta(0.0f);

    if (m_mouseCapture && m_mouseCapture->isCursorLocked()) {
        // Use mouse capture deltas (only when cursor is locked)
        mouseDelta.x = static_cast<float>(m_mouseCapture->getCursorDeltaX());
        mouseDelta.y = static_cast<float>(m_mouseCapture->getCursorDeltaY());
    } else if (!m_mouseCapture) {
        // Fall back to InputSystem for backward compatibility
        mouseDelta = InputSystem::getMouseDelta();
    }
    // If cursor is unlocked, mouseDelta remains (0, 0) - no camera rotation

    if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
        // Apply Minecraft's mouse sensitivity formula
        // sensitivity ranges from 0.0 to 1.0 (our slider is 1-100%, converted to 0.01-1.0)
        float d = m_mouseSensitivity * 0.6f + 0.2f;  // Map to 0.2-0.8 range
        float e = d * d * d;  // Cube for non-linear response
        float f = e * 8.0f;   // Final multiplier

        // Apply multiplier and final scaling (0.15 from Minecraft's Entity.changeLookDirection)
        float yawDelta = mouseDelta.x * f * 0.15f;
        float pitchDelta = -mouseDelta.y * f * 0.15f; // Inverted Y
        rotate(yawDelta, pitchDelta);
    }

    // Reset mouse capture deltas after processing
    if (m_mouseCapture) {
        m_mouseCapture->resetDeltas();
    }
}

void Camera::move(const glm::vec3& direction, float deltaTime) {
    m_position += direction * m_moveSpeed * deltaTime;
    updateViewMatrix();
}

void Camera::rotate(float yaw, float pitch) {
    m_yaw += yaw;
    m_pitch += pitch;

    // Constrain pitch to avoid gimbal lock
    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);

    updateVectors();
    updateViewMatrix();
}

void Camera::updateViewMatrix() {
    m_viewMatrix = glm::lookAt(m_position, m_position + m_forward, m_up);
}

void Camera::updateProjectionMatrix() {
    // Vulkan uses inverted Y clip space compared to OpenGL
    m_projectionMatrix = glm::perspective(glm::radians(m_fov), m_aspectRatio, m_nearPlane, m_farPlane);
    m_projectionMatrix[1][1] *= -1; // Flip Y for Vulkan
}

void Camera::updateVectors() {
    // Calculate forward vector from yaw and pitch
    glm::vec3 forward;
    forward.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    forward.y = sin(glm::radians(m_pitch));
    forward.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_forward = glm::normalize(forward);

    // Calculate right and up vectors
    m_right = glm::normalize(glm::cross(m_forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    m_up = glm::normalize(glm::cross(m_right, m_forward));
}

} // namespace VoxelEngine