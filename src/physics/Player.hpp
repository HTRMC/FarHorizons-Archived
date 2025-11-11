#pragma once

#include "AABB.hpp"
#include "CollisionSystem.hpp"
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

namespace FarHorizon {

// Player entity with physics simulation
// Based on Minecraft's PlayerEntity and LivingEntity classes
class Player {
public:
    // Minecraft player dimensions
    static constexpr float PLAYER_WIDTH = 0.6f;
    static constexpr float PLAYER_HEIGHT = 1.8f;
    static constexpr float PLAYER_EYE_HEIGHT = 1.62f;

    // Physics constants (from Minecraft)
    static constexpr float GRAVITY = 0.08f;  // Blocks per tick^2
    static constexpr float TERMINAL_VELOCITY = 3.92f;  // Max fall speed
    static constexpr float STEP_HEIGHT = 0.6f;  // Can step up 0.6 blocks (like slabs)

    // Movement constants
    static constexpr float WALK_SPEED = 4.317f;  // Blocks per second
    static constexpr float SPRINT_SPEED = 5.612f;
    static constexpr float SNEAK_SPEED = 1.295f;
    static constexpr float JUMP_VELOCITY = 0.42f;  // Initial jump velocity

    // Drag coefficients
    static constexpr float GROUND_DRAG = 0.91f;
    static constexpr float AIR_DRAG = 0.98f;

private:
    glm::dvec3 position_;          // Position of feet (current tick)
    glm::dvec3 previousPosition_;  // Position from previous tick (for interpolation)
    glm::dvec3 velocity_;          // Current velocity
    bool onGround_;                // Standing on a surface
    bool wasOnGround_;             // Was on ground last tick
    float stepHeight_;             // Current step height
    bool noClip_;                  // Ghost mode (fly through blocks)
    bool isSprinting_;
    bool isSneaking_;
    bool jumping_;                 // Jump input (set every frame, checked every tick)
    int jumpingCooldown_;          // Cooldown to prevent jump spam (in ticks)

public:
    Player()
        : position_(0, 100, 0)
        , previousPosition_(0, 100, 0)
        , velocity_(0, 0, 0)
        , onGround_(false)
        , wasOnGround_(false)
        , stepHeight_(STEP_HEIGHT)
        , noClip_(false)
        , isSprinting_(false)
        , isSneaking_(false)
        , jumping_(false)
        , jumpingCooldown_(0)
    {}

    // Getters
    const glm::dvec3& getPosition() const { return position_; }
    glm::dvec3 getEyePosition() const { return position_ + glm::dvec3(0, PLAYER_EYE_HEIGHT, 0); }
    const glm::dvec3& getVelocity() const { return velocity_; }
    bool isOnGround() const { return onGround_; }
    bool isNoClip() const { return noClip_; }
    float getStepHeight() const { return stepHeight_; }

    // Get interpolated eye position for smooth rendering (Minecraft's pattern)
    // partialTick: value from 0.0 to 1.0 representing progress between ticks
    // Used by renderer to provide buttery-smooth camera motion at high framerates
    glm::dvec3 getInterpolatedEyePosition(float partialTick) const {
        // Lerp between previous and current position
        glm::dvec3 interpolatedPos = glm::mix(previousPosition_, position_, static_cast<double>(partialTick));
        return interpolatedPos + glm::dvec3(0, PLAYER_EYE_HEIGHT, 0);
    }

    // Setters
    void setPosition(const glm::dvec3& pos) { position_ = pos; }
    void setVelocity(const glm::dvec3& vel) { velocity_ = vel; }
    void setNoClip(bool noClip) { noClip_ = noClip; }
    void setSprinting(bool sprinting) { isSprinting_ = sprinting; }
    void setSneaking(bool sneaking) { isSneaking_ = sneaking; }

    // Set jump input (called every frame, Minecraft's pattern)
    void setJumping(bool jumping) { jumping_ = jumping; }

    // Get player's bounding box
    AABB getBoundingBox() const {
        return AABB::fromCenter(
            position_ + glm::dvec3(0, PLAYER_HEIGHT / 2.0, 0),
            PLAYER_WIDTH, PLAYER_HEIGHT, PLAYER_WIDTH
        );
    }

    // Apply movement input (before physics)
    // input: normalized movement direction in local space (x=strafe, z=forward)
    // yaw: camera yaw in radians
    // Called once per tick (not per frame!)
    void applyMovementInput(const glm::vec2& input, float yaw) {
        if (glm::length(input) < 0.001f) return;

        // Minecraft's player movement speed attribute (default = 0.1)
        constexpr float MOVEMENT_SPEED = 0.1f;

        // Transform input to world space
        glm::vec2 normalizedInput = glm::normalize(input);
        float forward = normalizedInput.y;
        float strafe = normalizedInput.x;

        // Apply yaw rotation - Minecraft's exact formula (Entity.java line 1613)
        // In Minecraft: velocityX = strafe * cos(yaw) - forward * sin(yaw)
        //               velocityZ = forward * cos(yaw) + strafe * sin(yaw)
        // But our coordinate system has X and Z swapped compared to Minecraft
        float moveZ = strafe * cos(yaw) + forward * sin(yaw);
        float moveX = forward * cos(yaw) - strafe * sin(yaw);

        // In noclip mode, use creative flying speed
        if (noClip_) {
            // Creative flying uses higher speed multiplier
            float flyingSpeed = MOVEMENT_SPEED * 10.0f; // Much faster in creative
            velocity_.x = moveX * flyingSpeed;
            velocity_.z = moveZ * flyingSpeed;
        } else {
            // Minecraft's movement speed calculation
            // Ground: movementSpeed * (0.21600002 / slipperiness^3)
            // Air: movementSpeed * 0.1 (for players)
            float slipperiness = 0.6f; // Default block slipperiness
            float speed = onGround_
                ? MOVEMENT_SPEED * (0.21600002f / (slipperiness * slipperiness * slipperiness))
                : MOVEMENT_SPEED * 0.1f; // Air control

            // Sprinting multiplier (Minecraft uses 1.3x)
            if (isSprinting_) speed *= 1.3f;
            if (isSneaking_) speed *= 0.3f;

            // updateVelocity from Minecraft - adds to existing velocity
            velocity_.x += moveX * speed;
            velocity_.z += moveZ * speed;
        }
    }

    // Physics tick - apply gravity, drag, and collision
    // Based on Minecraft's LivingEntity.travel() and Entity.move()
    // Called exactly once per tick (20 times per second)
    void tick(CollisionSystem& collisionSystem) {
        // Save position before tick for interpolation (Minecraft's lastRenderX/Y/Z pattern)
        previousPosition_ = position_;
        wasOnGround_ = onGround_;

        if (noClip_) {
            // Creative flying mode - no physics, just direct movement
            position_ += velocity_;
            velocity_ *= AIR_DRAG; // Slow down over time
            return;
        }

        // Apply gravity (Minecraft applies this before movement)
        // Gravity is applied every tick when not on ground
        if (!onGround_) {
            velocity_.y -= GRAVITY;
            // Terminal velocity
            if (velocity_.y < -TERMINAL_VELOCITY) {
                velocity_.y = -TERMINAL_VELOCITY;
            }
        }

        // Velocity IS the movement (no deltaTime scaling needed - we run at fixed 20Hz)
        glm::dvec3 movement = velocity_;

        // Perform collision detection and resolution
        AABB currentBox = getBoundingBox();
        glm::dvec3 actualMovement = collisionSystem.adjustMovementForCollisionsWithStepping(
            currentBox, movement, stepHeight_
        );

        // Apply movement
        position_ += actualMovement;

        // Ground detection - Minecraft's exact logic (Entity.java line 724-726)
        // verticalCollision = movement.y != vec3d.y
        // groundCollision = verticalCollision && movement.y < 0.0
        bool verticalCollision = std::abs(movement.y - actualMovement.y) > AABB::EPSILON;
        onGround_ = verticalCollision && movement.y < 0.0;

        // Zero Y velocity if we hit something vertically
        if (verticalCollision) {
            velocity_.y = 0.0;
        }

        // Handle jump cooldown (Minecraft's pattern - LivingEntity.java line 2755-2757)
        if (jumpingCooldown_ > 0) {
            jumpingCooldown_--;
        }

        // Handle jump input (Minecraft's pattern - LivingEntity.java line 2822-2825)
        // Check if jumping flag is set AND we're on ground AND cooldown expired
        if (jumping_ && onGround_ && jumpingCooldown_ == 0) {
            jump();
            jumpingCooldown_ = 10;  // 10 ticks = 0.5 seconds (Minecraft's value)
        }

        // Apply drag (different for ground vs air)
        // Minecraft applies slipperiness * 0.91 for ground, 0.98 for air
        // Default block slipperiness = 0.6, so ground drag = 0.6 * 0.91 = 0.546
        float slipperiness = 0.6f; // Default block slipperiness
        float drag = onGround_ ? (slipperiness * 0.91f) : AIR_DRAG;
        velocity_.x *= drag;
        velocity_.z *= drag;

        // If horizontal movement was blocked, zero horizontal velocity
        if (std::abs(movement.x) > AABB::EPSILON &&
            std::abs(actualMovement.x) < AABB::EPSILON) {
            velocity_.x = 0.0;
        }
        if (std::abs(movement.z) > AABB::EPSILON &&
            std::abs(actualMovement.z) < AABB::EPSILON) {
            velocity_.z = 0.0;
        }

        // Zero out very small velocities
        if (std::abs(velocity_.x) < 0.003) velocity_.x = 0.0;
        if (std::abs(velocity_.y) < 0.003) velocity_.y = 0.0;
        if (std::abs(velocity_.z) < 0.003) velocity_.z = 0.0;
    }

    // Jump (can only jump when on ground)
    void jump() {
        if (onGround_ && !noClip_) {
            velocity_.y = JUMP_VELOCITY;
            onGround_ = false;
        }
    }

    // Teleport player to a position
    void teleport(const glm::dvec3& pos) {
        position_ = pos;
        velocity_ = glm::dvec3(0.0);
        onGround_ = false;
    }

    // Debug info
    bool wasOnGroundLastTick() const { return wasOnGround_; }
};

} // namespace FarHorizon