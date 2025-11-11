#pragma once

#include "Entity.hpp"
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

namespace FarHorizon {

// Living entity with jump mechanics and advanced physics
// Based on Minecraft's LivingEntity.java class
class LivingEntity : public Entity {
public:
    // Movement constants (from Minecraft)
    static constexpr float STEP_HEIGHT = 0.6f;  // Can step up 0.6 blocks (like slabs)
    static constexpr float JUMP_VELOCITY = 0.42f;  // Initial jump velocity

    // Drag coefficients (Minecraft's values)
    static constexpr float GROUND_SLIPPERINESS = 0.6f;
    static constexpr float AIR_SLIPPERINESS = 1.0f;
    static constexpr float DRAG_MULTIPLIER = 0.91f;
    static constexpr float VERTICAL_DRAG = 0.98f;

protected:
    float stepHeight_;             // Current step height
    bool isSprinting_;
    bool isSneaking_;
    bool jumping_;                 // Jump input (set every frame, checked every tick)
    int jumpingCooldown_;          // Cooldown to prevent jump spam (in ticks)

public:
    LivingEntity(const glm::dvec3& position = glm::dvec3(0, 100, 0))
        : Entity(position)
        , stepHeight_(STEP_HEIGHT)
        , isSprinting_(false)
        , isSneaking_(false)
        , jumping_(false)
        , jumpingCooldown_(0)
    {}

    virtual ~LivingEntity() = default;

    // Getters
    float getStepHeight() const { return stepHeight_; }
    bool isSprinting() const { return isSprinting_; }
    bool isSneaking() const { return isSneaking_; }

    // Setters
    void setSprinting(bool sprinting) { isSprinting_ = sprinting; }
    void setSneaking(bool sneaking) { isSneaking_ = sneaking; }
    void setJumping(bool jumping) { jumping_ = jumping; }

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
            float slipperiness = GROUND_SLIPPERINESS;
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

    // Physics tick - Minecraft's exact travel() structure
    // Based on LivingEntity.travelMidAir() lines 2309-2332
    // Called exactly once per tick (20 times per second)
    void tick(CollisionSystem& collisionSystem) override {
        if (noClip_) {
            // Creative flying mode - no physics, just direct movement
            previousPosition_ = position_;
            position_ += velocity_;
            velocity_ *= 0.91f;
            return;
        }

        // Minecraft's travelMidAir() - line 2310-2312
        // Get block slipperiness (0.6 on ground, 1.0 in air)
        float slipperiness = onGround_ ? GROUND_SLIPPERINESS : AIR_SLIPPERINESS;
        float drag = slipperiness * DRAG_MULTIPLIER;  // Ground: 0.546, Air: 0.91

        // Step 1: applyMovementInput() - This happens BEFORE gravity!
        // (Movement input was already applied in applyMovementInput(), velocity is ready)

        // Step 2: Move entity with collision (Minecraft line 2482: this.move())
        move(collisionSystem, stepHeight_);

        // Step 3: Apply gravity AFTER movement (Minecraft line 2319)
        if (!onGround_) {
            velocity_.y -= GRAVITY;
            if (velocity_.y < -TERMINAL_VELOCITY) {
                velocity_.y = -TERMINAL_VELOCITY;
            }
        }

        // Step 4: Handle jump (Minecraft line 2822-2825)
        if (jumpingCooldown_ > 0) {
            jumpingCooldown_--;
        }
        if (jumping_ && onGround_ && jumpingCooldown_ == 0) {
            jump();
            jumpingCooldown_ = 10;
        }

        // Step 5: Apply drag to ALL axes (Minecraft line 2330)
        // Horizontal: drag (0.546 ground, 0.91 air)
        // Vertical: 0.98 (always, unless Flutterer)
        velocity_.x *= drag;
        velocity_.z *= drag;
        velocity_.y *= VERTICAL_DRAG;

        // Zero out very small velocities
        if (std::abs(velocity_.x) < 0.003) velocity_.x = 0.0;
        if (std::abs(velocity_.y) < 0.003) velocity_.y = 0.0;
        if (std::abs(velocity_.z) < 0.003) velocity_.z = 0.0;
    }

protected:
    // Jump (can only jump when on ground)
    // Based on Minecraft's LivingEntity.jump() method
    virtual void jump() {
        if (onGround_ && !noClip_) {
            velocity_.y = JUMP_VELOCITY;
            onGround_ = false;
        }
    }
};

} // namespace FarHorizon