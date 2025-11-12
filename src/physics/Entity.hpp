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

    // Collision flags (Entity.java line 722-726)
    bool horizontalCollision;
    bool verticalCollision;
    bool groundCollision;         // Minecraft's name for onGround
    bool collidedSoftly;

    bool noClip;                  // Ghost mode (fly through blocks)1

public:
    Entity(const glm::dvec3& position = glm::dvec3(0, 100, 0))
        : pos(position)
        , lastRenderPos(position)
        , velocity(0, 0, 0)
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
};

} // namespace FarHorizon