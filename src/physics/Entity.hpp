#pragma once

#include "AABB.hpp"
#include "EntityType.hpp"
#include "EntityDimensions.hpp"
#include "util/MathHelper.hpp"
#include <glm/glm.hpp>
#include <optional>
#include <vector>
#include <memory>

namespace FarHorizon {

// Forward declarations
class VoxelShape;
class Level;

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
    bool m_horizontalCollision;           // Any horizontal collision (X or Z)
    bool m_verticalCollision;             // Any vertical collision (Y)
    bool m_verticalCollisionBelow;        // Vertical collision while moving down (Entity.java:779)
    bool m_minorHorizontalCollision;      // Small horizontal collision (Entity.java:784)
    bool m_onGround;                      // On ground flag (Entity.java)
    bool m_onGroundNoBlocks;              // On ground but no supporting blocks found (Entity.java)

protected:
    glm::dvec3 m_position;                // Current position (Entity.java uses x, y, z)
    glm::dvec3 m_lastRenderPos;      // Position from previous tick (for interpolation)
    glm::dvec3 m_velocity;           // Current velocity

    // Rotation (Entity.java line 210-213)
    float m_yaw;                     // Horizontal rotation (degrees)
    float m_pitch;                   // Vertical rotation (degrees)
    float m_lastYaw;                 // Previous yaw for interpolation
    float m_lastPitch;               // Previous pitch for interpolation

    bool m_collidedSoftly;
    bool m_noClip;                  // Ghost mode (fly through blocks)

private:
    // Entity type (Entity.java line 198 - private final EntityType<?> type)
    const EntityType m_type;

    // Entity ID (Entity.java line 200)
    int m_id;

    // Precise position flag (Entity.java line 199)
    bool m_requiresPrecisePosition;

    // Entity dimensions (Entity.java line 196 - private EntityDimensions dimensions)
    EntityDimensions m_dimensions;

    // Last known position (Entity.java: private BlockPos lastKnownPosition)
    // Used for tracking position changes and optimizations
    std::optional<glm::dvec3> m_lastKnownPosition;

    // Last known speed (Entity.java: private Vec3 lastKnownSpeed)
    // Calculated in computeSpeed() as position delta
    glm::dvec3 m_lastKnownSpeed;

    // Main supporting block position (Entity.java: private Optional<BlockPos> mainSupportingBlockPos)
    // The block position that is currently supporting this entity
    std::optional<glm::ivec3> m_mainSupportingBlockPos;

public:
    // Constructor matching Minecraft's Entity(EntityType<?> type, Level level)
    Entity(EntityType entityType, EntityDimensions dimensions, const glm::dvec3& position = glm::dvec3(0, 100, 0))
        : m_type(entityType)
        , m_id(-1)  // Will be set when added to world
        , m_requiresPrecisePosition(false)
        , m_dimensions(dimensions)
        , m_lastKnownPosition(std::nullopt)
        , m_lastKnownSpeed(0.0, 0.0, 0.0)
        , m_mainSupportingBlockPos(std::nullopt)
        , m_position(position)
        , m_lastRenderPos(position)
        , m_velocity(0, 0, 0)
        , m_yaw(0.0f)
        , m_pitch(0.0f)
        , m_lastYaw(0.0f)
        , m_lastPitch(0.0f)
        , m_horizontalCollision(false)
        , m_verticalCollision(false)
        , m_onGround(false)
        , m_onGroundNoBlocks(false)
        , m_verticalCollisionBelow(false)
        , m_minorHorizontalCollision(false)
        , m_collidedSoftly(false)
        , m_noClip(false)
    {}

    virtual ~Entity() = default;

    // Getters
    const glm::dvec3& getPos() const { return m_position; }
    double getX() const { return m_position.x; }
    double getY() const { return m_position.y; }
    double getZ() const { return m_position.z; }
    const glm::dvec3& getVelocity() const { return m_velocity; }
    float getYaw() const { return m_yaw; }
    float getPitch() const { return m_pitch; }
    float getYRot() const;  // Entity.java: public float getYRot()
    float getXRot() const;  // Entity.java: public float getXRot()
    bool isNoClip() const { return m_noClip; }
    bool isOnGround() const { return m_onGround; }  // Entity.java: public boolean isOnGround()

    // Get entity type (Entity.java: public EntityType<?> getType())
    EntityType getType() const { return m_type; }

    // Get entity ID (Entity.java: public int getId())
    int getId() const { return m_id; }

    // Get precise position requirement (Entity.java: public boolean getRequiresPrecisePosition())
    bool getRequiresPrecisePosition() const { return m_requiresPrecisePosition; }

    // Get entity dimensions (Entity.java: public EntityDimensions getDimensions())
    const EntityDimensions& getDimensions() const { return m_dimensions; }

    // Get interpolated position for smooth rendering (Minecraft's pattern)
    // partialTick: value from 0.0 to 1.0 representing progress between ticks
    // Minecraft uses lastRenderX/Y/Z for interpolation
    glm::dvec3 getLerpedPos(float partialTick) const {
        return glm::mix(m_lastRenderPos, m_position, static_cast<double>(partialTick));
    }

    // Setters
    void setPos(const glm::dvec3& position);  // Entity.java: public final void setPos(Vec3 pos)
    void setPos(double x, double y, double z);  // Entity.java: public void setPos(double x, double y, double z)

    void setVelocity(const glm::dvec3& vel) { m_velocity = vel; }
    void setVelocity(double x, double y, double z) { m_velocity = glm::dvec3(x, y, z); }

    void setYaw(float yawDegrees) { m_yaw = yawDegrees; }
    void setPitch(float pitchDegrees) { m_pitch = pitchDegrees; }

    // Turn entity based on mouse input (Entity.java: public void turn(double xo, double yo))
    void turn(double xo, double yo);

    void setNoClip(bool noclip) { m_noClip = noclip; }

    // Set on ground state (Entity.java: public void setOnGround(boolean onGround))
    void setOnGround(bool onGround);

    // Set on ground with movement (Entity.java: public void setOnGroundWithMovement(boolean onGround, Vec3 movement))
    void setOnGroundWithMovement(bool onGround, const glm::dvec3& movement);

    // Check if entity is supported by a specific block position (Entity.java: public boolean isSupportedBy(BlockPos pos))
    bool isSupportedBy(const glm::ivec3& pos) const;

    // Set entity ID (Entity.java: public void setId(int id))
    void setId(int id) { m_id = id; }

    // Set precise position requirement (Entity.java: public void setRequiresPrecisePosition(boolean))
    void setRequiresPrecisePosition(bool requiresPrecisePosition) {
        m_requiresPrecisePosition = requiresPrecisePosition;
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

    // Check if entity is free to move to a position (Entity.java: public boolean isFree(double xa, double ya, double za))
    bool isFree(double xa, double ya, double za) const;

protected:
    // Save position for interpolation (called at start of tick)
    void updateLastRenderPos();

    // Compute speed by calculating position delta (Entity.java: protected void computeSpeed())
    void computeSpeed();

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

    // Check and update supporting block (Entity.java: protected void checkSupportingBlock(boolean onGround, Vec3 movement))
    void checkSupportingBlock(bool onGround, const glm::dvec3* movement);

    // Transform movement input to velocity based on entity rotation (Entity.java line 1605)
    static glm::dvec3 movementInputToVelocity(const glm::dvec3& movementInput, float speed, float yawDegrees);

    // Static collision methods (Entity.java lines 1137-1180)

    // Collide bounding box with world (Entity.java line 1137: public static Vec3 collideBoundingBox)
    static glm::dvec3 collideBoundingBox(const Entity* source, const glm::dvec3& movement, const AABB& boundingBox,
                                         Level* level, const std::vector<std::shared_ptr<VoxelShape>>& entityColliders);

    // Collect all colliders in bounding box (Entity.java line 1142: public static List<VoxelShape> collectAllColliders)
    static std::vector<std::shared_ptr<VoxelShape>> collectAllColliders(const Entity* source, Level* level, const AABB& boundingBox);

    // Collect colliders (Entity.java line 1147: private static List<VoxelShape> collectColliders)
    static std::vector<std::shared_ptr<VoxelShape>> collectColliders(const Entity* source, Level* level,
                                                                      const std::vector<std::shared_ptr<VoxelShape>>& entityColliders,
                                                                      const AABB& boundingBox);

    // Collect candidate step up heights (Entity.java line 1110: private static float[] collectCandidateStepUpHeights)
    static std::vector<float> collectCandidateStepUpHeights(const AABB& boundingBox,
                                                             const std::vector<std::shared_ptr<VoxelShape>>& colliders,
                                                             float maxStepHeight, float stepHeightToSkip);

    // Collide with shapes (Entity.java line 1163: private static Vec3 collideWithShapes)
    static glm::dvec3 collideWithShapes(const glm::dvec3& movement, const AABB& boundingBox,
                                        const std::vector<std::shared_ptr<VoxelShape>>& shapes);

private:
    // Private collision method (Entity.java line 1076)
    // This is the core collision logic that handles both basic collision and stepping
    glm::dvec3 collide(const glm::dvec3& movement, Level* level);

    // Check if bounding box is free (Entity.java: private boolean isFree(AABB box))
    bool isFree(const AABB& box) const;

    // Set on ground with movement (3-parameter version)
    void setOnGroundWithMovement(bool onGround, bool horizontalCollision, const glm::dvec3& movement);
};

} // namespace FarHorizon