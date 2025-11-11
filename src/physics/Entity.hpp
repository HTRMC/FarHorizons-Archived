#pragma once

#include "AABB.hpp"
#include "CollisionSystem.hpp"
#include <glm/glm.hpp>

namespace FarHorizon {

// Base entity class with core physics
// Based on Minecraft's Entity.java class
class Entity {
public:
    // Physics constants (from Minecraft Entity.java)
    static constexpr float GRAVITY = 0.08f;  // Blocks per tick^2
    static constexpr float TERMINAL_VELOCITY = 3.92f;  // Max fall speed

protected:
    glm::dvec3 position_;          // Position (current tick)
    glm::dvec3 previousPosition_;  // Position from previous tick (for interpolation)
    glm::dvec3 velocity_;          // Current velocity
    bool onGround_;                // Standing on a surface
    bool wasOnGround_;             // Was on ground last tick
    bool noClip_;                  // Ghost mode (fly through blocks)

public:
    Entity(const glm::dvec3& position = glm::dvec3(0, 100, 0))
        : position_(position)
        , previousPosition_(position)
        , velocity_(0, 0, 0)
        , onGround_(false)
        , wasOnGround_(false)
        , noClip_(false)
    {}

    virtual ~Entity() = default;

    // Getters
    const glm::dvec3& getPosition() const { return position_; }
    const glm::dvec3& getVelocity() const { return velocity_; }
    bool isOnGround() const { return onGround_; }
    bool isNoClip() const { return noClip_; }

    // Get interpolated position for smooth rendering (Minecraft's pattern)
    // partialTick: value from 0.0 to 1.0 representing progress between ticks
    glm::dvec3 getInterpolatedPosition(float partialTick) const {
        return glm::mix(previousPosition_, position_, static_cast<double>(partialTick));
    }

    // Setters
    void setPosition(const glm::dvec3& pos) { position_ = pos; }
    void setVelocity(const glm::dvec3& vel) { velocity_ = vel; }
    void setNoClip(bool noClip) { noClip_ = noClip; }

    // Get entity's bounding box (abstract - subclasses define dimensions)
    virtual AABB getBoundingBox() const = 0;

    // Physics tick - subclasses implement their own physics behavior
    virtual void tick(CollisionSystem& collisionSystem) = 0;

    // Teleport entity to a position
    void teleport(const glm::dvec3& pos) {
        position_ = pos;
        previousPosition_ = pos;
        velocity_ = glm::dvec3(0.0);
        onGround_ = false;
    }

    // Debug info
    bool wasOnGroundLastTick() const { return wasOnGround_; }

protected:
    // Core movement with collision detection
    // Based on Minecraft's Entity.move() method (Entity.java line 724-726)
    // This is called by subclasses during their tick
    void move(CollisionSystem& collisionSystem, float stepHeight) {
        // Save position before movement for interpolation
        previousPosition_ = position_;
        wasOnGround_ = onGround_;

        if (noClip_) {
            // Creative flying mode - no physics, just direct movement
            position_ += velocity_;
            velocity_ *= 0.91f;
            return;
        }

        // Move entity with collision (Minecraft line 2482: this.move())
        glm::dvec3 movement = velocity_;
        AABB currentBox = getBoundingBox();
        glm::dvec3 actualMovement = collisionSystem.adjustMovementForCollisionsWithStepping(
            currentBox, movement, stepHeight
        );
        position_ += actualMovement;

        // Ground detection - Minecraft's exact logic (Entity.java line 724-726)
        bool verticalCollision = std::abs(movement.y - actualMovement.y) > AABB::EPSILON;
        onGround_ = verticalCollision && movement.y < 0.0;

        // Zero velocity if blocked
        if (verticalCollision) {
            velocity_.y = 0.0;
        }
        if (std::abs(movement.x) > AABB::EPSILON && std::abs(actualMovement.x) < AABB::EPSILON) {
            velocity_.x = 0.0;
        }
        if (std::abs(movement.z) > AABB::EPSILON && std::abs(actualMovement.z) < AABB::EPSILON) {
            velocity_.z = 0.0;
        }
    }
};

} // namespace FarHorizon