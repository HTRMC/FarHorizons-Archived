#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "InputTypes.hpp"
#include "KeybindAction.hpp"
#include "MouseCapture.hpp"
#include <unordered_map>
#include <string>

namespace FarHorizon {

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
    void setMouseSensitivity(float sensitivity) { mouseSensitivity_ = sensitivity; }

    // Set mouse capture system (optional - if not set, uses InputSystem directly)
    void setMouseCapture(MouseCapture* mouseCapture) { mouseCapture_ = mouseCapture; }

    // Update camera (call each frame with delta time)
    void update(float deltaTime);

    // Movement and rotation
    void move(const glm::vec3& direction, float deltaTime);
    void rotate(float yaw, float pitch); // In degrees

    // Setters
    void setPosition(const glm::vec3& position) { position_ = position; updateViewMatrix(); }
    void setAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; updateProjectionMatrix(); }
    void setFov(float fov) { fov_ = fov; updateProjectionMatrix(); }
    void setMoveSpeed(float speed) { moveSpeed_ = speed; }
    void setRotationSpeed(float speed) { rotationSpeed_ = speed; }

    // Getters
    const glm::vec3& getPosition() const { return position_; }
    const glm::vec3& getForward() const { return forward_; }
    const glm::vec3& getRight() const { return right_; }
    const glm::vec3& getUp() const { return up_; }
    const glm::mat4& getViewMatrix() const { return viewMatrix_; }
    const glm::mat4& getProjectionMatrix() const { return projectionMatrix_; }
    glm::mat4 getViewProjectionMatrix() const { return projectionMatrix_ * viewMatrix_; }

    // Get rotation-only view-projection matrix for camera-relative rendering
    // (translation is handled by subtracting camera position in shader)
    glm::mat4 getRotationOnlyViewProjectionMatrix() const {
        glm::mat4 rotationOnlyView = glm::lookAt(glm::vec3(0.0f), forward_, up_);
        return projectionMatrix_ * rotationOnlyView;
    }

    float getYaw() const { return yaw_; }
    float getPitch() const { return pitch_; }
    float getFov() const { return fov_; }

private:
    void updateViewMatrix();
    void updateProjectionMatrix();
    void updateVectors();

private:
    // Camera position and orientation
    glm::vec3 position_ = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 forward_ = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 right_ = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 up_ = glm::vec3(0.0f, 1.0f, 0.0f);

    // Euler angles (in degrees)
    // Using Minecraft's coordinate system: Yaw 0° = +Z (south)
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;

    // Projection parameters
    float fov_ = 70.0f;
    float aspectRatio_ = 16.0f / 9.0f;
    float nearPlane_ = 0.1f;
    float farPlane_ = 1000.0f;

    // Movement parameters
    float moveSpeed_ = 5.0f;        // Units per second
    float rotationSpeed_ = 90.0f;   // Degrees per second
    float mouseSensitivity_ = 0.1f; // Mouse sensitivity multiplier

    // Matrices
    glm::mat4 viewMatrix_ = glm::mat4(1.0f);
    glm::mat4 projectionMatrix_ = glm::mat4(1.0f);

    // Parsed keybinds (for fast lookup during update)
    KeyCode keyForward_ = KeyCode::W;
    KeyCode keyBack_ = KeyCode::S;
    KeyCode keyLeft_ = KeyCode::A;
    KeyCode keyRight_ = KeyCode::D;
    KeyCode keyJump_ = KeyCode::Space;
    KeyCode keySneak_ = KeyCode::LeftShift;

    // Mouse capture system (optional - if not set, uses InputSystem)
    MouseCapture* mouseCapture_ = nullptr;
};

} // namespace FarHorizon