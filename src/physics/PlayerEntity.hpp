#pragma once

#include "LivingEntity.hpp"
#include <glm/glm.hpp>

namespace FarHorizon {

// Player entity with player-specific properties
// Based on Minecraft's PlayerEntity and ClientPlayerEntity classes
class PlayerEntity : public LivingEntity {
public:
    // Minecraft player dimensions
    static constexpr float PLAYER_WIDTH = 0.6f;
    static constexpr float PLAYER_HEIGHT = 1.8f;
    static constexpr float PLAYER_EYE_HEIGHT = 1.62f;

    // Movement speed constants (for reference, actual speeds calculated in LivingEntity)
    static constexpr float WALK_SPEED = 4.317f;  // Blocks per second
    static constexpr float SPRINT_SPEED = 5.612f;
    static constexpr float SNEAK_SPEED = 1.295f;

private:
    float yaw;  // Camera yaw for movement rotation

public:
    PlayerEntity(const glm::dvec3& position = glm::dvec3(0, 100, 0))
        : LivingEntity(position)
        , yaw(0.0f)
    {}

    // Set camera yaw for movement rotation
    void setYaw(float yawRadians) {
        yaw = yawRadians;
    }

    // Get player's bounding box
    AABB getBoundingBox() const override {
        return AABB::fromCenter(
            m_position + glm::dvec3(0, PLAYER_HEIGHT / 2.0, 0),
            PLAYER_WIDTH, PLAYER_HEIGHT, PLAYER_WIDTH
        );
    }

    // Get eye position for camera
    glm::dvec3 getEyePos() const {
        return m_position + glm::dvec3(0, PLAYER_EYE_HEIGHT, 0);
    }

    // Get interpolated eye position for smooth rendering (Minecraft's pattern)
    // partialTick: value from 0.0 to 1.0 representing progress between ticks
    // Used by renderer to provide buttery-smooth camera motion at high framerates
    glm::dvec3 getLerpedEyePos(float partialTick) const {
        // Lerp between previous and current position
        glm::dvec3 interpolatedPos = getLerpedPos(partialTick);
        return interpolatedPos + glm::dvec3(0, PLAYER_EYE_HEIGHT, 0);
    }

protected:
    // Override updateVelocity to apply yaw rotation (Entity.java line 1593-1617)
    // Transforms local movement input (forward/strafe) to world space using camera yaw
    void updateVelocity(float speed, const glm::dvec3& movementInput) override {
        if (glm::length(movementInput) < 0.001f) return;

        // Minecraft uses: sideways (x), upward (y), forward (z)
        float strafe = movementInput.x;
        float upward = movementInput.y;
        float forward = movementInput.z;

        // Normalize input vector if needed (Minecraft line 1595-1603)
        float lengthSquared = strafe * strafe + forward * forward;
        if (lengthSquared >= 1.0E-4f) {
            float length = std::sqrt(lengthSquared);
            if (length < 1.0f) {
                length = 1.0f;
            }
            float scale = speed / length;
            strafe *= scale;
            forward *= scale;

            // Apply yaw rotation - Minecraft's exact formula (Entity.java line 1613)
            // In Minecraft: velocityX = strafe * cos(yaw) - forward * sin(yaw)
            //               velocityZ = forward * cos(yaw) + strafe * sin(yaw)
            // But our coordinate system has X and Z swapped compared to Minecraft
            float moveZ = strafe * cos(yaw) + forward * sin(yaw);
            float moveX = forward * cos(yaw) - strafe * sin(yaw);

            m_velocity.x += moveX;
            m_velocity.z += moveZ;
        }

        // Apply upward movement (for creative flying)
        m_velocity.y += upward * speed;
    }
};

} // namespace FarHorizon