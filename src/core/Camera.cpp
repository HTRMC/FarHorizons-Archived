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
    // Parse keybind strings to KeyCode enums once
    auto getKey = [&](const std::string& action, KeyCode defaultKey) -> KeyCode {
        auto it = keybinds.find(action);
        if (it != keybinds.end()) {
            KeyCode parsed = InputSystem::stringToKeyCode(it->second);
            return (parsed != KeyCode::Unknown) ? parsed : defaultKey;
        }
        return defaultKey;
    };

    m_keyForward = getKey("key.forward", KeyCode::W);
    m_keyBack = getKey("key.back", KeyCode::S);
    m_keyLeft = getKey("key.left", KeyCode::A);
    m_keyRight = getKey("key.right", KeyCode::D);
    m_keyJump = getKey("key.jump", KeyCode::Space);
    m_keySneak = getKey("key.sneak", KeyCode::LeftShift);
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

    // Handle arrow key rotation (use isKeyPressed for held keys)
    float yawDelta = 0.0f;
    float pitchDelta = 0.0f;

    if (InputSystem::isKeyPressed(KeyCode::Left)) {
        yawDelta -= m_rotationSpeed * deltaTime;
    }
    if (InputSystem::isKeyPressed(KeyCode::Right)) {
        yawDelta += m_rotationSpeed * deltaTime;
    }
    if (InputSystem::isKeyPressed(KeyCode::Up)) {
        pitchDelta += m_rotationSpeed * deltaTime;
    }
    if (InputSystem::isKeyPressed(KeyCode::Down)) {
        pitchDelta -= m_rotationSpeed * deltaTime;
    }

    if (yawDelta != 0.0f || pitchDelta != 0.0f) {
        rotate(yawDelta, pitchDelta);
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