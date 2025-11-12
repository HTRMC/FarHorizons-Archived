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
    // Movement input (LivingEntity.java has these as fields)
    float forwardSpeed;    // Forward/backward movement input
    float sidewaysSpeed;   // Left/right movement input
    float upwardSpeed;     // Up/down movement input

    bool jumping;          // Jump input flag (LivingEntity.java line 2799)
    int jumpingCooldown;   // Cooldown to prevent jump spam (LivingEntity.java line 2755)

    bool sprinting;
    bool sneaking;

public:
    LivingEntity(const glm::dvec3& position = glm::dvec3(0, 100, 0))
        : Entity(position)
        , forwardSpeed(0.0f)
        , sidewaysSpeed(0.0f)
        , upwardSpeed(0.0f)
        , jumping(false)
        , jumpingCooldown(0)
        , sprinting(false)
        , sneaking(false)
    {}

    virtual ~LivingEntity() = default;

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

    // Main tick method (LivingEntity.java line 2545)
    // Minecraft: tick() calls super.tick() then tickMovement()
    void tick(CollisionSystem& collisionSystem) override {
        Entity::tick(collisionSystem);  // Call super.tick() (Entity.baseTick())
        tickMovement(collisionSystem);
    }

    // Main movement tick (LivingEntity.java line 2754)
    // This is where all the movement logic happens
    virtual void tickMovement(CollisionSystem& collisionSystem) {
        // Decrease jumping cooldown (LivingEntity.java line 2755-2757)
        if (jumpingCooldown > 0) {
            jumpingCooldown--;
        }

        // Zero out very small velocities (LivingEntity.java line 2771-2794)
        // IMPORTANT: Players use horizontalLengthSquared check (line 2775-2778)
        glm::dvec3 vel = getVelocity();

        // For players: zero horizontal if combined length squared < 9.0E-6
        double horizontalLengthSq = vel.x * vel.x + vel.z * vel.z;
        if (horizontalLengthSq < 9.0E-6) {  // 0.003 * 0.003 = 9.0E-6
            vel.x = 0.0;
            vel.z = 0.0;
        }

        // Always zero Y if too small
        if (std::abs(vel.y) < 0.003) {
            vel.y = 0.0;
        }

        setVelocity(vel);

        // Jump logic (LivingEntity.java line 2809-2834)
        if (jumping && groundCollision && jumpingCooldown == 0) {
            jump();
            jumpingCooldown = 10;  // 10 ticks = 0.5 seconds
        }

        // Travel with movement input (LivingEntity.java line 2851)
        glm::dvec3 movementInput(sidewaysSpeed, upwardSpeed, forwardSpeed);
        travel(movementInput, collisionSystem);

        // Debug logging: Print every 1 ticks (20 times per second at 20 TPS)
        static int debugTick = 0;
        if (++debugTick >= 1) {
            debugTick = 0;
            glm::dvec3 finalVel = getVelocity();
            spdlog::info("pos({:.3f}, {:.3f}, {:.3f}) vel({:.3f}, {:.3f}, {:.3f}) onGround={}",
                pos.x, pos.y, pos.z,
                finalVel.x, finalVel.y, finalVel.z,
                groundCollision);
        }
    }

    // Main travel method (LivingEntity.java line 2278)
    // Decides which travel mode to use (water/lava/gliding/mid-air)
    virtual void travel(const glm::dvec3& movementInput, CollisionSystem& collisionSystem) {
        // For now, only implement mid-air travel
        // Full implementation would check for water/lava/gliding
        travelMidAir(movementInput, collisionSystem);
    }

protected:
    // Mid-air travel (LivingEntity.java line 2309)
    // This is the core physics for walking/flying
    virtual void travelMidAir(const glm::dvec3& movementInput, CollisionSystem& collisionSystem) {
        updateLastRenderPos();  // Save position for interpolation

        if (noClip) {
            // Creative flying mode (simplified)
            glm::dvec3 vel = getVelocity();
            setPos(pos + vel);
            setVelocity(vel * 0.91);
            return;
        }

        // Get block slipperiness (LivingEntity.java line 2310-2312)
        // 0.6 on ground (default block), 1.0 in air
        float slipperiness = groundCollision ? GROUND_SLIPPERINESS : AIR_SLIPPERINESS;
        float drag = slipperiness * DRAG_MULTIPLIER;  // Ground: 0.546, Air: 0.91

        // Apply movement input and move (LivingEntity.java line 2313)
        // This calls updateVelocity() then move()
        glm::dvec3 newVelocity = applyMovementInput(movementInput, slipperiness, collisionSystem);

        // Apply gravity AFTER movement (LivingEntity.java line 2314-2324)
        double yVelocity = newVelocity.y;
        if (!groundCollision) {
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

    // Apply movement input (LivingEntity.java line 2479)
    // Calls updateVelocity() then move()
    virtual glm::dvec3 applyMovementInput(const glm::dvec3& movementInput, float slipperiness,
                                          CollisionSystem& collisionSystem) {
        // Calculate movement speed (LivingEntity.java line 2480)
        float movementSpeed = getMovementSpeed(slipperiness);

        // Update velocity with movement input (updateVelocity)
        updateVelocity(movementSpeed, movementInput);

        // Move with collision (LivingEntity.java line 2482)
        move(MovementType::SELF, getVelocity(), collisionSystem, STEP_HEIGHT);

        return getVelocity();
    }

    // Calculate movement speed based on slipperiness (from getMovementSpeed)
    virtual float getMovementSpeed(float slipperiness) const {
        // Minecraft's player movement speed attribute (default = 0.1)
        constexpr float MOVEMENT_SPEED = 0.1f;

        // Minecraft's movement speed calculation
        // Ground: movementSpeed * (0.21600002 / slipperiness^3)
        // Air: movementSpeed * 0.1 (for players)
        float speed = groundCollision
            ? MOVEMENT_SPEED * (0.21600002f / (slipperiness * slipperiness * slipperiness))
            : MOVEMENT_SPEED * 0.1f;  // Air control

        // Sprinting multiplier (Minecraft uses 1.3x)
        if (sprinting) speed *= 1.3f;
        if (sneaking) speed *= 0.3f;

        return speed;
    }

    // Update velocity from movement input (Entity.java line 1600)
    // Transforms local movement input to world space and adds to velocity
    virtual void updateVelocity(float speed, const glm::dvec3& movementInput) {
        // Use Entity's movementInputToVelocity to properly rotate by yaw (Entity.java line 1601)
        glm::dvec3 rotatedVelocity = movementInputToVelocity(movementInput, speed, yaw);

        // Add to current velocity (Entity.java line 1602)
        setVelocity(velocity + rotatedVelocity);
    }

    // Jump (LivingEntity.java has this as protected)
    virtual void jump() {
        if (groundCollision && !noClip) {
            velocity.y = JUMP_VELOCITY;
            groundCollision = false;
        }
    }
};

} // namespace FarHorizon