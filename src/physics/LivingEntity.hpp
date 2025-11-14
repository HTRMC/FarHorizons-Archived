#pragma once

#include "Entity.hpp"
#include <glm/glm.hpp>

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
    // Entity dimensions (Minecraft's EntityDimensions)
    float width;    // Bounding box width (X and Z)
    float height;   // Bounding box height (Y)

    // Movement input (LivingEntity.java has these as fields)
    float forwardSpeed;    // Forward/backward movement input
    float sidewaysSpeed;   // Left/right movement input
    float upwardSpeed;     // Up/down movement input

    bool jumping;          // Jump input flag (LivingEntity.java line 2799)
    int jumpingCooldown;   // Cooldown to prevent jump spam (LivingEntity.java line 2755)

    bool sprinting;
    bool sneaking;

public:
    // Constructor with dimensions (default to player size: 0.6 x 1.8)
    LivingEntity(EntityType entityType = EntityType::LIVING_ENTITY,
                 const glm::dvec3& position = glm::dvec3(0, 100, 0),
                 float entityWidth = 0.6f, float entityHeight = 1.8f)
        : Entity(entityType, EntityDimensions::scalable(entityWidth, entityHeight), position)
        , width(entityWidth)
        , height(entityHeight)
        , forwardSpeed(0.0f)
        , sidewaysSpeed(0.0f)
        , upwardSpeed(0.0f)
        , jumping(false)
        , jumpingCooldown(0)
        , sprinting(false)
        , sneaking(false)
    {}

    virtual ~LivingEntity() = default;

    // Get step height (Entity.java: maxUpStep())
    float getStepHeight() const override {
        return STEP_HEIGHT;
    }

    // Getters
    bool isSprinting() const { return sprinting; }
    bool isSneaking() const { return sneaking; }

    // Setters
    void setSprinting(bool sprint) { sprinting = sprint; }
    void setSneaking(bool sneak) { sneaking = sneak; }
    void setJumping(bool jump) { jumping = jump; }

    // Set movement input (called from input system)
    void setMovementInput(float forward, float sideways, float upward = 0.0f) {
        forwardSpeed = forward;
        sidewaysSpeed = sideways;
        upwardSpeed = upward;
    }

    // Main tick method (LivingEntity.java line 2632)
    // Minecraft: tick() calls super.tick() then aiStep()
    void tick(Level* level) {
        Entity::tick();  // Call super.tick() with no args (Entity.java line 2633)
        tickMovement(level);
    }

    // aiStep() - Main movement tick (LivingEntity.java line 2913)
    // This is where all the movement logic happens
    virtual void tickMovement(Level* level) {
        // Decrease jumping cooldown (aiStep line 2914-2916)
        if (jumpingCooldown > 0) {
            --jumpingCooldown;
        }

        // Get deltaMovement (aiStep line 2930)
        glm::dvec3 var1 = getVelocity();
        double var2 = var1.x;
        double var4 = var1.y;
        double var6 = var1.z;

        // Zero out very small velocities (aiStep line 2934-2947)
        // IMPORTANT: Players use horizontalDistanceSqr check
        if (true) {  // EntityType.PLAYER check
            double horizontalDistanceSqr = var1.x * var1.x + var1.z * var1.z;
            if (horizontalDistanceSqr < 9.0E-6) {
                var2 = 0.0;
                var6 = 0.0;
            }
        }

        // Always zero Y if too small (aiStep line 2949-2951)
        if (std::abs(var1.y) < 0.003) {
            var4 = 0.0;
        }

        // Set cleaned velocity (aiStep line 2953)
        setVelocity(var2, var4, var6);

        // Jump logic (aiStep line 2969-2991)
        // Simplified - just ground jump for now
        if (jumping) {
            if (onGround_ && jumpingCooldown == 0) {
                jumpFromGround();
                jumpingCooldown = 10;
            }
        } else {
            jumpingCooldown = 0;
        }

        // Travel with movement input (aiStep line 2994-3015)
        glm::dvec3 var10(sidewaysSpeed, upwardSpeed, forwardSpeed);
        travel(var10, level);
    }

    // Main travel method (LivingEntity.java line 2318)
    // Decides which travel mode to use (water/lava/gliding/mid-air)
    virtual void travel(const glm::dvec3& movementInput, Level* level) {
        // For now, only implement mid-air travel (travelInAir in Minecraft line 2354)
        // Full implementation would check for water/lava/gliding (line 2319-2325)
        travelMidAir(movementInput, level);
    }

protected:
    // Mid-air travel (LivingEntity.java line 2354: travelInAir)
    // This is the core physics for walking/flying
    virtual void travelMidAir(const glm::dvec3& movementInput, Level* level) {
        updateLastRenderPos();  // Save position for interpolation

        if (noClip_) {
            // Creative flying mode (simplified)
            glm::dvec3 vel = getVelocity();
            setPos(position_ + vel);
            setVelocity(vel * 0.91);
            return;
        }

        // Get block slipperiness (LivingEntity.java line 2356: getFriction from block below)
        // 0.6 on ground (default block), 1.0 in air
        float slipperiness = onGround_ ? GROUND_SLIPPERINESS : AIR_SLIPPERINESS;
        float drag = slipperiness * DRAG_MULTIPLIER;  // Ground: 0.546, Air: 0.91

        // Apply movement input and move (LivingEntity.java line 2358: handleRelativeFrictionAndCalculateMovement)
        // This calls moveRelative (updateVelocity) then move()
        glm::dvec3 newVelocity = applyMovementInput(movementInput, slipperiness, level);

        // Apply gravity AFTER movement (LivingEntity.java line 2359-2371: effective gravity)
        double yVelocity = newVelocity.y;
        if (!onGround_) {
            yVelocity -= GRAVITY;
            if (yVelocity < -TERMINAL_VELOCITY) {
                yVelocity = -TERMINAL_VELOCITY;
            }
        }

        // Apply drag (LivingEntity.java line 2326-2331)
        // Horizontal drag depends on slipperiness, vertical is always 0.98
        float verticalDrag = VERTICAL_DRAG;  // 0.98 always (unless Flutterer)
        setVelocity(newVelocity.x * drag, yVelocity * verticalDrag, newVelocity.z * drag);
    }

    // Apply movement input (LivingEntity.java line 2556: handleRelativeFrictionAndCalculateMovement)
    // Calls moveRelative (updateVelocity) then move()
    virtual glm::dvec3 applyMovementInput(const glm::dvec3& movementInput, float slipperiness,
                                          Level* level) {
        // Calculate movement speed (LivingEntity.java line 2557: getFrictionInfluencedSpeed)
        float movementSpeed = getMovementSpeed(slipperiness);

        // Update velocity with movement input (LivingEntity.java line 2557: moveRelative)
        updateVelocity(movementSpeed, movementInput);

        // Move with collision (LivingEntity.java line 2559)
        move(MovementType::SELF, getVelocity(), level);

        return getVelocity();
    }

    // Calculate movement speed based on slipperiness (LivingEntity.java line 2600: getFrictionInfluencedSpeed)
    virtual float getMovementSpeed(float slipperiness) const {
        // Minecraft's player movement speed attribute (default = 0.1)
        constexpr float MOVEMENT_SPEED = 0.1f;

        // Minecraft's movement speed calculation (line 2601)
        // Ground: speed * (0.21600002 / friction^3)
        // Air: getFlyingSpeed() which returns speed * 0.1 for players (line 2604-2605)
        float speed = onGround_
            ? MOVEMENT_SPEED * (0.21600002f / (slipperiness * slipperiness * slipperiness))
            : MOVEMENT_SPEED * 0.1f;  // Air control (getFlyingSpeed)

        // Sprinting multiplier (Minecraft uses 1.3x)
        if (sprinting) speed *= 1.3f;
        if (sneaking) speed *= 0.3f;

        return speed;
    }

    // Update velocity from movement input (LivingEntity.java line 2557: moveRelative, which calls Entity.moveRelative)
    // Transforms local movement input to world space and adds to velocity
    // Entity.java line 1600: moveRelative(float speed, Vec3 input)
    virtual void updateVelocity(float speed, const glm::dvec3& movementInput) {
        // Use Entity's movementInputToVelocity to properly rotate by yaw (Entity.java line 1605)
        glm::dvec3 rotatedVelocity = movementInputToVelocity(movementInput, speed, yaw_);

        // Add to current velocity (Entity.java line 1606)
        setVelocity(velocity_ + rotatedVelocity);
    }

    // Jump from ground (LivingEntity.java line 2279: jumpFromGround)
    virtual void jumpFromGround() {
        if (onGround_ && !noClip_) {
            // Use Math.max to preserve upward velocity if already moving up faster
            // (LivingEntity.java line 2283: Math.max((double)var1, var2.y))
            velocity_.y = std::max(static_cast<double>(JUMP_VELOCITY), velocity_.y);
            onGround_ = false;

            // Add sprint jump boost (LivingEntity.java line 2284-2287)
            if (sprinting) {
                float yawRadians = yaw_ * (glm::pi<float>() / 180.0f);
                velocity_.x += -std::sin(yawRadians) * 0.2;
                velocity_.z += std::cos(yawRadians) * 0.2;
            }
        }
    }
};

} // namespace FarHorizon