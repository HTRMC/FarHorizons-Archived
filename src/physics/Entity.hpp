#pragma once

#include "AABB.hpp"
#include "CollisionSystem.hpp"
#include "util/MathHelper.hpp"
#include <glm/glm.hpp>

namespace FarHorizon {

// MovementType enum (from Minecraft Entity.java)
enum class MovementType {
    SELF,
    PLAYER,
    PISTON,
    SHULKER_BOX,
    SHULKER
};

// Base entity class with core physics
// Based on Minecraft's Entity.java class
class Entity {
public:
    // Physics constants (from Minecraft Entity.java)
    static constexpr float GRAVITY = 0.08f;  // Blocks per tick^2
    static constexpr float TERMINAL_VELOCITY = 3.92f;  // Max fall speed

protected:
    glm::dvec3 pos;                // Current position (Entity.java uses x, y, z)
    glm::dvec3 lastRenderPos;      // Position from previous tick (for interpolation)
    glm::dvec3 velocity;           // Current velocity

    // Rotation (Entity.java line 210-213)
    float yaw;                     // Horizontal rotation (degrees)
    float pitch;                   // Vertical rotation (degrees)
    float lastYaw;                 // Previous yaw for interpolation
    float lastPitch;               // Previous pitch for interpolation

    // Collision flags (Entity.java line 722-726)
    bool horizontalCollision;
    bool verticalCollision;
    bool groundCollision;         // Minecraft's name for onGround
    bool collidedSoftly;

    bool noClip;                  // Ghost mode (fly through blocks)

public:
    Entity(const glm::dvec3& position = glm::dvec3(0, 100, 0))
        : pos(position)
        , lastRenderPos(position)
        , velocity(0, 0, 0)
        , yaw(0.0f)
        , pitch(0.0f)
        , lastYaw(0.0f)
        , lastPitch(0.0f)
        , horizontalCollision(false)
        , verticalCollision(false)
        , groundCollision(false)
        , collidedSoftly(false)
        , noClip(false)
    {}

    virtual ~Entity() = default;

    // Getters
    const glm::dvec3& getPos() const { return pos; }
    double getX() const { return pos.x; }
    double getY() const { return pos.y; }
    double getZ() const { return pos.z; }
    const glm::dvec3& getVelocity() const { return velocity; }
    float getYaw() const { return yaw; }
    float getPitch() const { return pitch; }
    bool isOnGround() const { return groundCollision; }
    bool isNoClip() const { return noClip; }

    // Get interpolated position for smooth rendering (Minecraft's pattern)
    // partialTick: value from 0.0 to 1.0 representing progress between ticks
    // Minecraft uses lastRenderX/Y/Z for interpolation
    glm::dvec3 getLerpedPos(float partialTick) const {
        return glm::mix(lastRenderPos, pos, static_cast<double>(partialTick));
    }

    // Setters
    void setPos(double x, double y, double z) { pos = glm::dvec3(x, y, z); }
    void setPos(const glm::dvec3& position) { pos = position; }
    void setVelocity(const glm::dvec3& vel) { velocity = vel; }
    void setVelocity(double x, double y, double z) { velocity = glm::dvec3(x, y, z); }
    void setYaw(float yawDegrees) { yaw = yawDegrees; }
    void setPitch(float pitchDegrees) { pitch = pitchDegrees; }
    void setRotation(float yawDegrees, float pitchDegrees) {
        yaw = yawDegrees;
        pitch = pitchDegrees;
    }
    void setNoClip(bool noclip) { noClip = noclip; }

    // Get entity's bounding box (abstract - subclasses define dimensions)
    virtual AABB getBoundingBox() const = 0;

    // Main tick method - called once per game tick (Entity.java line 477)
    // Minecraft: tick() calls baseTick()
    virtual void tick(CollisionSystem& collisionSystem) {
        baseTick();
    }

    // Base tick - handles basic entity updates (Entity.java line 481)
    virtual void baseTick() {
        // In full implementation: fire ticks, portal logic, water state, etc.
        // For now, minimal implementation
    }

    // Core movement with collision detection (Entity.java line 672)
    // Minecraft: move(MovementType type, Vec3d movement)
    void move(MovementType type, const glm::dvec3& movement, CollisionSystem& collisionSystem, float stepHeight) {
        if (noClip) {
            // Creative flying mode - no collision (Entity.java line 673-678)
            setPos(pos + movement);
            horizontalCollision = false;
            verticalCollision = false;
            groundCollision = false;
            collidedSoftly = false;
            return;
        }

        // adjustMovementForCollisions (Entity.java line 699)
        AABB currentBox = getBoundingBox();
        glm::dvec3 actualMovement = collisionSystem.adjustMovementForCollisionsWithStepping(
            currentBox, movement, stepHeight
        );

        // Update position (Entity.java line 715)
        setPos(pos + actualMovement);

        // Collision detection (Entity.java line 720-726)
        // Minecraft uses approximatelyEquals for X/Z, exact comparison for Y
        bool xBlocked = !MathHelper::approximatelyEquals(movement.x, actualMovement.x);
        bool zBlocked = !MathHelper::approximatelyEquals(movement.z, actualMovement.z);
        horizontalCollision = xBlocked || zBlocked;
        verticalCollision = movement.y != actualMovement.y;  // EXACT comparison for Y!
        groundCollision = verticalCollision && movement.y < 0.0;

        // Reset velocity on collision (Entity.java line 745-746)
        if (horizontalCollision) {
            glm::dvec3 vel = getVelocity();
            setVelocity(xBlocked ? 0.0 : vel.x, vel.y, zBlocked ? 0.0 : vel.z);
        }
    }

    // Teleport entity to a position
    void teleport(const glm::dvec3& position) {
        pos = position;
        lastRenderPos = position;
        velocity = glm::dvec3(0.0);
        groundCollision = false;
    }

protected:
    // Save position for interpolation (called at start of tick)
    void updateLastRenderPos() {
        lastRenderPos = pos;
    }

    // Transform movement input to velocity based on entity rotation (Entity.java line 1605)
    // movementInput: local movement (sideways, up, forward)
    // speed: movement speed multiplier
    // yawDegrees: entity's yaw rotation in degrees
    static glm::dvec3 movementInputToVelocity(const glm::dvec3& movementInput, float speed, float yawDegrees) {
        double lengthSquared = glm::dot(movementInput, movementInput);

        // Early exit for zero movement (Entity.java line 1607)
        if (lengthSquared < 1.0E-7) {
            return glm::dvec3(0.0);
        }

        // Normalize if length > 1.0 (Entity.java line 1610)
        glm::dvec3 normalized = (lengthSquared > 1.0) ? glm::normalize(movementInput) : movementInput;
        glm::dvec3 scaled = normalized * static_cast<double>(speed);

        // Rotate by yaw angle (Entity.java line 1611-1613)
        // Convert yaw from degrees to radians
        float yawRadians = yawDegrees * (glm::pi<float>() / 180.0f);
        float sinYaw = std::sin(yawRadians);
        float cosYaw = std::cos(yawRadians);

        // Apply rotation matrix (Entity.java line 1613)
        // new Vec3d(vec3d.x * g - vec3d.z * f, vec3d.y, vec3d.z * g + vec3d.x * f)
        // where f = sin(yaw), g = cos(yaw)
        return glm::dvec3(
            scaled.x * cosYaw - scaled.z * sinYaw,  // Rotated X
            scaled.y,                                 // Y unchanged
            scaled.z * cosYaw + scaled.x * sinYaw   // Rotated Z
        );
    }
};

} // namespace FarHorizon