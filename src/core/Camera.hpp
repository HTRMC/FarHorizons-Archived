#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "InputTypes.hpp"
#include "KeybindAction.hpp"
#include <unordered_map>
#include <string>

namespace VoxelEngine {

class MouseCapture;

// First-person camera with customizable keybinds
class Camera {
public:
    Camera() = default;
    ~Camera() = default;

    // Initialize camera with position and aspect ratio
    void init(const glm::vec3& position, float aspectRatio, float fov = 70.0f);

    // Set keybinds (call once at startup or when keybinds change)
    void setKeybinds(const std::unordered_map<std::string, std::string>& keybinds);

    // Set mouse sensitivity
    void setMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }

    // Set mouse capture system (optional - if not set, uses InputSystem directly)
    void setMouseCapture(MouseCapture* mouseCapture) { m_mouseCapture = mouseCapture; }

    // Update camera (call each frame with delta time)
    void update(float deltaTime);

    // Movement and rotation
    void move(const glm::vec3& direction, float deltaTime);
    void rotate(float yaw, float pitch); // In degrees

    // Setters
    void setPosition(const glm::vec3& position) { m_position = position; updateViewMatrix(); }
    void setAspectRatio(float aspectRatio) { m_aspectRatio = aspectRatio; updateProjectionMatrix(); }
    void setFov(float fov) { m_fov = fov; updateProjectionMatrix(); }
    void setMoveSpeed(float speed) { m_moveSpeed = speed; }
    void setRotationSpeed(float speed) { m_rotationSpeed = speed; }

    // Getters
    const glm::vec3& getPosition() const { return m_position; }
    const glm::vec3& getForward() const { return m_forward; }
    const glm::vec3& getRight() const { return m_right; }
    const glm::vec3& getUp() const { return m_up; }
    const glm::mat4& getViewMatrix() const { return m_viewMatrix; }
    const glm::mat4& getProjectionMatrix() const { return m_projectionMatrix; }
    glm::mat4 getViewProjectionMatrix() const { return m_projectionMatrix * m_viewMatrix; }

    // Get rotation-only view-projection matrix for camera-relative rendering
    // (translation is handled by subtracting camera position in shader)
    glm::mat4 getRotationOnlyViewProjectionMatrix() const {
        glm::mat4 rotationOnlyView = glm::lookAt(glm::vec3(0.0f), m_forward, m_up);
        return m_projectionMatrix * rotationOnlyView;
    }

    float getYaw() const { return m_yaw; }
    float getPitch() const { return m_pitch; }
    float getFov() const { return m_fov; }

private:
    void updateViewMatrix();
    void updateProjectionMatrix();
    void updateVectors();

private:
    // Camera position and orientation
    glm::vec3 m_position = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 m_forward = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 m_right = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 m_up = glm::vec3(0.0f, 1.0f, 0.0f);

    // Euler angles (in degrees)
    float m_yaw = -90.0f;   // Looking towards -Z
    float m_pitch = 0.0f;

    // Projection parameters
    float m_fov = 70.0f;
    float m_aspectRatio = 16.0f / 9.0f;
    float m_nearPlane = 0.1f;
    float m_farPlane = 1000.0f;

    // Movement parameters
    float m_moveSpeed = 5.0f;        // Units per second
    float m_rotationSpeed = 90.0f;   // Degrees per second
    float m_mouseSensitivity = 0.1f; // Mouse sensitivity multiplier

    // Matrices
    glm::mat4 m_viewMatrix = glm::mat4(1.0f);
    glm::mat4 m_projectionMatrix = glm::mat4(1.0f);

    // Parsed keybinds (for fast lookup during update)
    KeyCode m_keyForward = KeyCode::W;
    KeyCode m_keyBack = KeyCode::S;
    KeyCode m_keyLeft = KeyCode::A;
    KeyCode m_keyRight = KeyCode::D;
    KeyCode m_keyJump = KeyCode::Space;
    KeyCode m_keySneak = KeyCode::LeftShift;

    // Mouse capture system (optional - if not set, uses InputSystem)
    MouseCapture* m_mouseCapture = nullptr;
};

} // namespace VoxelEngine