#pragma once

#include "AABB.hpp"
#include "CollisionSystem.hpp"
#include "EntityType.hpp"
#include "EntityDimensions.hpp"
#include "util/MathHelper.hpp"
#include <glm/glm.hpp>
#include <optional>

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

public:
    // Collision flags (Entity.java line 219-221 - public fields)
    bool horizontalCollision;           // Any horizontal collision (X or Z)
    bool verticalCollision;             // Any vertical collision (Y)
    bool verticalCollisionBelow;        // Vertical collision while moving down (Entity.java:779)
    bool minorHorizontalCollision;      // Small horizontal collision (Entity.java:784)
    bool groundCollision;               // Minecraft's onGround

protected:
    glm::dvec3 pos;                // Current position (Entity.java uses x, y, z)
    glm::dvec3 lastRenderPos;      // Position from previous tick (for interpolation)
    glm::dvec3 velocity;           // Current velocity

    // Rotation (Entity.java line 210-213)
    float yaw;                     // Horizontal rotation (degrees)
    float pitch;                   // Vertical rotation (degrees)
    float lastYaw;                 // Previous yaw for interpolation
    float lastPitch;               // Previous pitch for interpolation

    bool collidedSoftly;
    bool noClip;                  // Ghost mode (fly through blocks)

private:
    // Entity type (Entity.java line 198 - private final EntityType<?> type)
    const EntityType type_;

    // Entity ID (Entity.java line 200)
    int id_;

    // Precise position flag (Entity.java line 199)
    bool requiresPrecisePosition_;

    // Entity dimensions (Entity.java line 196 - private EntityDimensions dimensions)
    EntityDimensions dimensions_;

    // Last known position (Entity.java: private BlockPos lastKnownPosition)
    // Used for tracking position changes and optimizations
    std::optional<glm::dvec3> lastKnownPosition_;

public:
    // Constructor matching Minecraft's Entity(EntityType<?> type, Level level)
    Entity(EntityType entityType, EntityDimensions dimensions, const glm::dvec3& position = glm::dvec3(0, 100, 0))
        : type_(entityType)
        , id_(-1)  // Will be set when added to world
        , requiresPrecisePosition_(false)
        , dimensions_(dimensions)
        , lastKnownPosition_(std::nullopt)
        , pos(position)
        , lastRenderPos(position)
        , velocity(0, 0, 0)
        , yaw(0.0f)
        , pitch(0.0f)
        , lastYaw(0.0f)
        , lastPitch(0.0f)
        , horizontalCollision(false)
        , verticalCollision(false)
        , groundCollision(false)
        , verticalCollisionBelow(false)
        , minorHorizontalCollision(false)
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
    float getYRot() const;  // Entity.java: public float getYRot()
    float getXRot() const;  // Entity.java: public float getXRot()
    bool isNoClip() const { return noClip; }
    bool isOnGround() const { return groundCollision; }  // Convenience getter

    // Get entity type (Entity.java: public EntityType<?> getType())
    EntityType getType() const { return type_; }

    // Get entity ID (Entity.java: public int getId())
    int getId() const { return id_; }

    // Get precise position requirement (Entity.java: public boolean getRequiresPrecisePosition())
    bool getRequiresPrecisePosition() const { return requiresPrecisePosition_; }

    // Get entity dimensions (Entity.java: public EntityDimensions getDimensions())
    const EntityDimensions& getDimensions() const { return dimensions_; }

    // Get interpolated position for smooth rendering (Minecraft's pattern)
    // partialTick: value from 0.0 to 1.0 representing progress between ticks
    // Minecraft uses lastRenderX/Y/Z for interpolation
    glm::dvec3 getLerpedPos(float partialTick) const {
        return glm::mix(lastRenderPos, pos, static_cast<double>(partialTick));
    }

    // Setters
    void setPos(const glm::dvec3& position);  // Entity.java: public final void setPos(Vec3 pos)
    void setPos(double x, double y, double z);  // Entity.java: public void setPos(double x, double y, double z)

    void setVelocity(const glm::dvec3& vel) { velocity = vel; }
    void setVelocity(double x, double y, double z) { velocity = glm::dvec3(x, y, z); }

    void setYaw(float yawDegrees) { yaw = yawDegrees; }
    void setPitch(float pitchDegrees) { pitch = pitchDegrees; }

    // Turn entity based on mouse input (Entity.java: public void turn(double xo, double yo))
    void turn(double xo, double yo);

    void setNoClip(bool noclip) { noClip = noclip; }

    // Set entity ID (Entity.java: public void setId(int id))
    void setId(int id) { id_ = id; }

    // Set precise position requirement (Entity.java: public void setRequiresPrecisePosition(boolean))
    void setRequiresPrecisePosition(bool requiresPrecisePosition) {
        requiresPrecisePosition_ = requiresPrecisePosition;
    }

    // Get entity's bounding box (abstract - subclasses define dimensions)
    virtual AABB getBoundingBox() const = 0;

    // Get maximum step height (Entity.java: maxUpStep())
    // Returns how high this entity can step up
    virtual float getStepHeight() const = 0;

    // Main tick method - called once per game tick (Entity.java line 477)
    virtual void tick();

    // Base tick - handles basic entity updates (Entity.java line 481)
    virtual void baseTick();

    // Core movement with collision detection (Entity.java line 672)
    // Minecraft: move(MovementType type, Vec3d movement)
    void move(MovementType type, const glm::dvec3& movement, CollisionSystem& collisionSystem);

    // Teleport entity to a position
    void teleport(const glm::dvec3& position);

    // Kill the entity (Entity.java: public void kill(ServerLevel level))
    // Placeholder - removes entity and triggers death event
    void kill();

    // Check if entity is colliding with a specific block (Entity.java line 339)
    // Minecraft signature: isColliding(BlockPos pos, BlockState state)
    // This checks if the entity's bounding box intersects with the block's collision shape
    bool isColliding(const glm::ivec3& blockPos, const BlockState& blockState, CollisionSystem& collisionSystem) const;

protected:
    // Save position for interpolation (called at start of tick)
    void updateLastRenderPos();

    // Create bounding box from current position and dimensions (Entity.java: protected AABB makeBoundingBox())
    AABB makeBoundingBox() const;

    // Reapply position to update bounding box (Entity.java: protected void reapplyPosition())
    void reapplyPosition();

    // Set rotation with wrapping (Entity.java: protected void setRot(float yRot, float xRot))
    // Wraps angles to 0-360 range
    void setRot(float yRot, float xRot);

    // Set yaw rotation (Entity.java: public void setYRot(float yRot))
    void setYRot(float yRot);

    // Set pitch rotation (Entity.java: public void setXRot(float xRot))
    // Clamps pitch to -90 to 90 degrees
    void setXRot(float xRot);

    // Transform movement input to velocity based on entity rotation (Entity.java line 1605)
    static glm::dvec3 movementInputToVelocity(const glm::dvec3& movementInput, float speed, float yawDegrees);

private:
    // Private collision method (Entity.java line 1076)
    // This is the core collision logic that handles both basic collision and stepping
    glm::dvec3 collide(const glm::dvec3& movement, CollisionSystem& collisionSystem);
};

} // namespace FarHorizon