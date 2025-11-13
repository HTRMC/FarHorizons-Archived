#pragma once

#include "AABB.hpp"
#include "EntityType.hpp"
#include "EntityDimensions.hpp"
#include "util/MathHelper.hpp"
#include "world/BlockState.hpp"
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
    bool horizontalCollision_;           // Any horizontal collision (X or Z)
    bool verticalCollision_;             // Any vertical collision (Y)
    bool verticalCollisionBelow_;        // Vertical collision while moving down (Entity.java:779)
    bool minorHorizontalCollision_;      // Small horizontal collision (Entity.java:784)
    bool onGround_;                      // On ground flag (Entity.java)
    bool onGroundNoBlocks_;              // On ground but no supporting blocks found (Entity.java)

protected:
    glm::dvec3 position_;                // Current position (Entity.java uses x, y, z)
    glm::dvec3 lastRenderPos_;      // Position from previous tick (for interpolation)
    glm::dvec3 velocity_;           // Current velocity

    // Rotation (Entity.java line 210-213)
    float yaw_;                     // Horizontal rotation (degrees)
    float pitch_;                   // Vertical rotation (degrees)
    float lastYaw_;                 // Previous yaw for interpolation
    float lastPitch_;               // Previous pitch for interpolation

    bool collidedSoftly_;
    bool noClip_;                  // Ghost mode (fly through blocks)

private:
    // Entity type (Entity.java line 198 - private final EntityType<?> type)
    const EntityType type_;

    // Entity ID (Entity.java line 200)
    int id_;

    // Precise position flag (Entity.java line 199)
    bool requiresPrecisePosition_;

    // Entity dimensions (Entity.java line 196 - private EntityDimensions dimensions)
    EntityDimensions dimensions_;

    // Bounding box (Entity.java line 195 - private AABB bb)
    AABB bb_;

    // Last known position (Entity.java: private BlockPos lastKnownPosition)
    // Used for tracking position changes and optimizations
    std::optional<glm::dvec3> lastKnownPosition_;

    // Last known speed (Entity.java: private Vec3 lastKnownSpeed)
    // Calculated in computeSpeed() as position delta
    glm::dvec3 lastKnownSpeed_;

    // Main supporting block position (Entity.java: private Optional<BlockPos> mainSupportingBlockPos)
    // The block position that is currently supporting this entity
    std::optional<glm::ivec3> mainSupportingBlockPos_;

    // Level reference (Entity.java: private Level level)
    Level* level_;

public:
    // Constructor matching Minecraft's Entity(EntityType<?> type, Level level)
    Entity(EntityType entityType, EntityDimensions dimensions, const glm::dvec3& position = glm::dvec3(0, 100, 0))
        : type_(entityType)
        , id_(-1)  // Will be set when added to world
        , requiresPrecisePosition_(false)
        , dimensions_(dimensions)
        , bb_(dimensions.makeBoundingBox(position))
        , lastKnownPosition_(std::nullopt)
        , lastKnownSpeed_(0.0, 0.0, 0.0)
        , mainSupportingBlockPos_(std::nullopt)
        , level_(nullptr)  // Will be set after construction
        , position_(position)
        , lastRenderPos_(position)
        , velocity_(0, 0, 0)
        , yaw_(0.0f)
        , pitch_(0.0f)
        , lastYaw_(0.0f)
        , lastPitch_(0.0f)
        , horizontalCollision_(false)
        , verticalCollision_(false)
        , onGround_(false)
        , onGroundNoBlocks_(false)
        , verticalCollisionBelow_(false)
        , minorHorizontalCollision_(false)
        , collidedSoftly_(false)
        , noClip_(false)
    {}

    virtual ~Entity() = default;

    // Getters
    const glm::dvec3& getPos() const { return position_; }
    double getX() const { return position_.x; }
    double getY() const { return position_.y; }
    double getZ() const { return position_.z; }
    const glm::dvec3& getVelocity() const { return velocity_; }
    float getYaw() const { return yaw_; }
    float getPitch() const { return pitch_; }
    float getYRot() const;  // Entity.java: public float getYRot()
    float getXRot() const;  // Entity.java: public float getXRot()
    bool isNoClip() const { return noClip_; }
    bool isOnGround() const { return onGround_; }  // Entity.java: public boolean isOnGround()

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
        return glm::mix(lastRenderPos_, position_, static_cast<double>(partialTick));
    }

    // Setters
    void setPos(const glm::dvec3& position);  // Entity.java: public final void setPos(Vec3 pos)
    void setPos(double x, double y, double z);  // Entity.java: public void setPos(double x, double y, double z)
    void setPosRaw(double x, double y, double z);  // Entity.java: public final void setPosRaw(double x, double y, double z)

    void setVelocity(const glm::dvec3& vel) { velocity_ = vel; }
    void setVelocity(double x, double y, double z) { velocity_ = glm::dvec3(x, y, z); }

    void setYaw(float yawDegrees) { yaw_ = yawDegrees; }
    void setPitch(float pitchDegrees) { pitch_ = pitchDegrees; }

    // Turn entity based on mouse input (Entity.java: public void turn(double xo, double yo))
    void turn(double xo, double yo);

    void setNoClip(bool noclip) { noClip_ = noclip; }

    // Set on ground state (Entity.java: public void setOnGround(boolean onGround))
    void setOnGround(bool onGround);

    // Set on ground with movement (Entity.java: public void setOnGroundWithMovement(boolean onGround, Vec3 movement))
    void setOnGroundWithMovement(bool onGround, const glm::dvec3& movement);

    // Check if entity is supported by a specific block position (Entity.java: public boolean isSupportedBy(BlockPos pos))
    bool isSupportedBy(const glm::ivec3& pos) const;

    // Set entity ID (Entity.java: public void setId(int id))
    void setId(int id) { id_ = id; }

    // Get level (Entity.java: public Level level())
    Level* level() const { 
        return level_; 
    }
    
    // Set level (Entity.java: Entity constructor sets level in initializer)
    void setLevel(Level* level) { 
        level_ = level; 
    }

    // Set precise position requirement (Entity.java: public void setRequiresPrecisePosition(boolean))
    void setRequiresPrecisePosition(bool requiresPrecisePosition) {
        requiresPrecisePosition_ = requiresPrecisePosition;
    }

    // Set bounding box (Entity.java: public final void setBoundingBox(AABB bb))
    void setBoundingBox(const AABB& bb) {
        bb_ = bb;
    }

    // Get entity's bounding box (Entity.java: public AABB getBoundingBox())
    AABB getBoundingBox() const {
        return bb_;
    }

    // Get maximum step height (Entity.java: maxUpStep())
    // Returns how high this entity can step up
    virtual float getStepHeight() const = 0;

    // Main tick method - called once per game tick (Entity.java line 477)
    virtual void tick();

    // Base tick - handles basic entity updates (Entity.java line 481)
    virtual void baseTick();

    // Core movement with collision detection (Entity.java line 672)
    // Minecraft: move(MovementType type, Vec3d movement)
    void move(MovementType type, const glm::dvec3& movement, Level* level);

    // Teleport entity to a position
    void teleport(const glm::dvec3& position);

    // Kill the entity (Entity.java: public void kill(ServerLevel level))
    // Placeholder - removes entity and triggers death event
    void kill();

    // Check if entity is colliding with a specific block (Entity.java line 339)
    // Minecraft signature: isColliding(BlockPos pos, BlockState state)
    // This checks if the entity's bounding box intersects with the block's collision shape
    bool isColliding(const glm::ivec3& blockPos, const BlockState& blockState) const;

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