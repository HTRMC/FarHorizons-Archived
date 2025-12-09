#include "Entity.hpp"
#include <tracy/Tracy.hpp>
#include <cmath>
#include <algorithm>
#include <glm/ext/scalar_constants.hpp>
#include <spdlog/spdlog.h>

#include "world/Level.hpp"
#include "world/BlockRegistry.hpp"
#include "voxel/Shapes.hpp"

namespace FarHorizon {

// Kill the entity (Entity.java: public void kill(ServerLevel level))
void Entity::kill() {
    // Placeholder implementation
    // In full implementation:
    // 1. this.remove(Entity.RemovalReason.KILLED) - marks entity for removal from world
    // 2. this.gameEvent(GameEvent.ENTITY_DIE) - triggers death game event for sculk sensors

    // TODO: Add removal flag when entity management system is implemented
    // TODO: Trigger game events when event system is implemented
}

// Set rotation with wrapping (Entity.java: protected void setRot(float yRot, float xRot))
void Entity::setRot(float yRot, float xRot) {
    setYRot(std::fmod(yRot, 360.0f));
    setXRot(std::fmod(xRot, 360.0f));
}

// Set position (Entity.java: public final void setPos(Vec3 pos))
void Entity::setPos(const glm::dvec3& position) {
    setPos(position.x, position.y, position.z);
}

// Set position (Entity.java: public void setPos(double x, double y, double z))
void Entity::setPos(double x, double y, double z) {
    setPosRaw(x, y, z);
    setBoundingBox(makeBoundingBox());
}

// Set raw position without updating bounding box (Entity.java: public final void setPosRaw(double x, double y, double z))
void Entity::setPosRaw(double x, double y, double z) {
    // Only update if position actually changed (Entity.java line 238)
    if (position_.x != x || position_.y != y || position_.z != z) {
        position_ = glm::dvec3(x, y, z);

        // Calculate block position (Entity.java line 240-242)
        int blockX = static_cast<int>(std::floor(x));
        int blockY = static_cast<int>(std::floor(y));
        int blockZ = static_cast<int>(std::floor(z));

        // Check if block position changed (Entity.java line 243)
        if (!lastKnownPosition_.has_value() ||
            blockX != static_cast<int>(std::floor(lastKnownPosition_->x)) ||
            blockY != static_cast<int>(std::floor(lastKnownPosition_->y)) ||
            blockZ != static_cast<int>(std::floor(lastKnownPosition_->z))) {

            // Update block position (Entity.java line 244)
            lastKnownPosition_ = glm::dvec3(blockX, blockY, blockZ);

            // TODO: Implement when these systems are available:
            // - Update chunk position if chunk changed (Entity.java line 245-251)
            // - Call LevelCallback.onMove() (Entity.java line 252)
            // - Call GameEventListenerRegistrar.onListenerMove() (Entity.java line 253)
            // - Update waypoint tracking (Entity.java line 254-257)
        }
    }
}

// Create bounding box from current position and dimensions (Entity.java: protected AABB makeBoundingBox())
AABB Entity::makeBoundingBox() const {
    return dimensions_.makeBoundingBox(position_);
}

// Reapply position to update bounding box (Entity.java: protected void reapplyPosition())
void Entity::reapplyPosition() {
    lastKnownPosition_ = std::nullopt;
    setPos(position_.x, position_.y, position_.z);
}

// Turn entity based on mouse input (Entity.java: public void turn(double xo, double yo))
void Entity::turn(double xo, double yo) {
    float var5 = static_cast<float>(yo) * 0.15f;
    float var6 = static_cast<float>(xo) * 0.15f;
    setXRot(getXRot() + var5);
    setYRot(getYRot() + var6);
    setXRot(std::clamp(getXRot(), -90.0f, 90.0f));
    lastPitch_ += var5;
    lastYaw_ += var6;
    lastPitch_ = std::clamp(lastPitch_, -90.0f, 90.0f);
    // Note: In Minecraft, if there's a vehicle, it calls vehicle.onPassengerTurned(this)
    // We don't have vehicle support yet
}

// Tick implementations (Entity.java line 477)
void Entity::tick() {
    ZoneScoped;
    baseTick();
}

void Entity::baseTick() {
    ZoneScoped;
    // Full Minecraft implementation (Entity.java baseTick line ~500):
    computeSpeed();  // Calculate position delta for this tick

    // Not yet implemented:
    // - inBlockState tracking
    // - Vehicle/passenger logic (isPassenger, getVehicle, stopRiding)
    // - boardingCooldown countdown
    // - handlePortal() - nether/end portal logic
    // - spawnSprintParticle() - sprint particles
    // - Powder snow tracking (wasInPowderSnow, isInPowderSnow)
    // - updateInWaterStateAndDoFluidPushing() - water physics
    // - updateFluidOnEyes() - check if head is in water/lava
    // - updateSwimming() - swimming state
    // - Fire tick logic (remainingFireTicks, fireImmune, damage from fire)
    // - Lava fall distance reduction (fallDistance *= 0.5)
    // - checkBelowWorld() - void damage
    // - setSharedFlagOnFire() - sync fire state
    // - firstTick flag reset
    // - Leashable tick (for leashed entities)
}

// Compute speed by calculating position delta (Entity.java: protected void computeSpeed())
void Entity::computeSpeed() {
    if (!lastKnownPosition_.has_value()) {
        lastKnownPosition_ = position_;
    }

    lastKnownSpeed_ = position_ - lastKnownPosition_.value();
    lastKnownPosition_ = position_;
}

// Check if entity is free to move to a position (Entity.java: public boolean isFree(double xa, double ya, double za))
bool Entity::isFree(double xa, double ya, double za) const {
    return isFree(getBoundingBox().move(xa, ya, za));
}

// Check if bounding box is free (Entity.java: private boolean isFree(AABB box))
bool Entity::isFree(const AABB& box) const {
    // Entity.java: return this.level().noCollision(this, box) && !this.level().containsAnyLiquid(box);
    return level_->noCollision(this, box) && !level_->containsAnyLiquid(box);
}

// Set on ground state (Entity.java: public void setOnGround(boolean onGround))
void Entity::setOnGround(bool onGround) {
    onGround_ = onGround;
    checkSupportingBlock(onGround, nullptr);
}

// Set on ground with movement (Entity.java: public void setOnGroundWithMovement(boolean onGround, Vec3 movement))
void Entity::setOnGroundWithMovement(bool onGround, const glm::dvec3& movement) {
    setOnGroundWithMovement(onGround, horizontalCollision_, movement);
}

// Set on ground with movement (3-parameter version)
void Entity::setOnGroundWithMovement(bool onGround, bool horizontalCollision, const glm::dvec3& movement) {
    onGround_ = onGround;
    horizontalCollision_ = horizontalCollision;
    checkSupportingBlock(onGround, &movement);
}

// Check if entity is supported by a specific block position (Entity.java: public boolean isSupportedBy(BlockPos pos))
bool Entity::isSupportedBy(const glm::ivec3& pos) const {
    return mainSupportingBlockPos_.has_value() && mainSupportingBlockPos_.value() == pos;
}

// Check and update supporting block (Entity.java: protected void checkSupportingBlock(boolean onGround, Vec3 movement))
void Entity::checkSupportingBlock(bool onGround, const glm::dvec3* movement) {
    if (onGround) {
        // Early exit if level is not set
        if (!level_) {
            onGroundNoBlocks_ = false;
            return;
        }

        // Get bounding box slightly below entity (Entity.java line 816: AABB var3 = this.getBoundingBox())
        AABB var3 = getBoundingBox();
        // Entity.java line 817: AABB var4 = new AABB(var3.minX, var3.minY - 1.0E-6, var3.minZ, var3.maxX, var3.minY, var3.maxZ)
        AABB var4(
            var3.minX, var3.minY - 1.0E-6, var3.minZ,
            var3.maxX, var3.minY, var3.maxZ
        );

        // Entity.java line 818: Optional var5 = this.level.findSupportingBlock(this, var4);
        std::optional<glm::ivec3> var5 = level_->findSupportingBlock(this, var4);

        // Entity.java line 819: if (!var5.isPresent() && !this.onGroundNoBlocks)
        if (!var5.has_value() && !onGroundNoBlocks_) {
            // Entity.java line 820: if (movement != null)
            if (movement != nullptr) {
                // Entity.java line 821: AABB var6 = var4.move(-movement.x, 0.0, -movement.z);
                AABB var6 = var4.move(-movement->x, 0.0, -movement->z);
                // Entity.java line 822: var5 = this.level.findSupportingBlock(this, var6);
                var5 = level_->findSupportingBlock(this, var6);
                // Entity.java line 823: this.mainSupportingBlockPos = var5;
                mainSupportingBlockPos_ = var5;
            }
        } else {
            // Entity.java line 825: this.mainSupportingBlockPos = var5;
            mainSupportingBlockPos_ = var5;
        }

        // Entity.java line 828: this.onGroundNoBlocks = var5.isEmpty();
        onGroundNoBlocks_ = !var5.has_value();
    } else {
        // Entity.java line 830: this.onGroundNoBlocks = false;
        onGroundNoBlocks_ = false;
        // Entity.java line 831: if (this.mainSupportingBlockPos.isPresent())
        if (mainSupportingBlockPos_.has_value()) {
            // Entity.java line 832: this.mainSupportingBlockPos = Optional.empty();
            mainSupportingBlockPos_ = std::nullopt;
        }
    }
}

// Set yaw rotation (Entity.java: public void setYRot(float yRot))
void Entity::setYRot(float yRot) {
    if (!std::isfinite(yRot)) {
        spdlog::warn("Invalid entity rotation: {}, discarding.", yRot);
    } else {
        this->yaw_ = yRot;
    }
}

// Set pitch rotation (Entity.java: public void setXRot(float xRot))
void Entity::setXRot(float xRot) {
    if (!std::isfinite(xRot)) {
        spdlog::warn("Invalid entity rotation: {}, discarding.", xRot);
    } else {
        this->pitch_ = std::clamp(std::fmod(xRot, 360.0f), -90.0f, 90.0f);
    }
}

void Entity::updateLastRenderPos() {
    lastRenderPos_ = position_;
}

// Get yaw rotation (Entity.java: public float getYRot())
float Entity::getYRot() const {
    return yaw_;
}

// Get pitch rotation (Entity.java: public float getXRot())
float Entity::getXRot() const {
    return pitch_;
}

void Entity::teleport(const glm::dvec3& position) {
    position_ = position;
    lastRenderPos_ = position;
    velocity_ = glm::dvec3(0.0);
    this->onGround_ = false;
}

// Private collision method (Entity.java line 1076)
// This is the core collision logic that handles both basic collision and stepping
glm::dvec3 Entity::collide(const glm::dvec3& movement, Level* level) {
    AABB entityBox = getBoundingBox();

    // Get entity collisions (Entity.java line 1078)
    std::vector<std::shared_ptr<VoxelShape>> entityCollisions = level->getEntityCollisions(this, entityBox.expandTowards(movement));

    // First try basic collision resolution (Entity.java line 1079)
    glm::dvec3 resolvedMovement = (movement.x * movement.x + movement.y * movement.y + movement.z * movement.z == 0.0)
        ? movement
        : collideBoundingBox(this, movement, entityBox, level, entityCollisions);

    // Check if movement was blocked (Entity.java line 1080-1083)
    bool xBlocked = movement.x != resolvedMovement.x;
    bool yBlocked = movement.y != resolvedMovement.y;
    bool zBlocked = movement.z != resolvedMovement.z;
    bool fallingAndHitGround = yBlocked && movement.y < 0.0;

    // Minecraft's stepping condition (Entity.java line 1084):
    // maxUpStep() > 0.0F && (fallingAndHitGround || this.onGround()) && (xBlocked || zBlocked)
    float stepHeight = getStepHeight();
    if (stepHeight > 0.0f &&
        (fallingAndHitGround || this->onGround_) &&
        (xBlocked || zBlocked)) {

        // Create starting box for step attempt (Entity.java line 1085)
        AABB stepBox = fallingAndHitGround
            ? entityBox.move(0.0, resolvedMovement.y, 0.0)
            : entityBox;

        // Expand box for step attempt (Entity.java line 1086-1088)
        AABB stepSweptBox = stepBox.expandTowards(movement.x, stepHeight, movement.z);
        if (!fallingAndHitGround) {
            stepSweptBox = stepSweptBox.expandTowards(0.0, -9.999999747378752E-6, 0.0);
        }

        // Get all colliders for stepping (Entity.java line 1091)
        auto stepCollisions = collectColliders(this, level, entityCollisions, stepSweptBox);

        // Collect candidate step heights (Entity.java line 1093)
        float currentY = static_cast<float>(resolvedMovement.y);
        std::vector<float> candidateHeights = collectCandidateStepUpHeights(
            stepBox, stepCollisions, stepHeight, currentY
        );

        // Try each step height (Entity.java line 1097-1104)
        for (float tryHeight : candidateHeights) {
            glm::dvec3 tryMovement(movement.x, tryHeight, movement.z);
            glm::dvec3 result = collideWithShapes(tryMovement, stepBox, stepCollisions);

            // Check if this gives better horizontal movement (Entity.java line 1100)
            if (result.x * result.x + result.z * result.z >
                resolvedMovement.x * resolvedMovement.x + resolvedMovement.z * resolvedMovement.z) {

                // Adjust for box offset (Entity.java line 1101-1102)
                double yOffset = entityBox.minY - stepBox.minY;
                return result - glm::dvec3(0.0, yOffset, 0.0);
            }
        }
    }

    return resolvedMovement;
}

// Main movement method (Entity.java line 672)
void Entity::move(MovementType type, const glm::dvec3& movement, Level* level) {
    if (noClip_) {
        // Creative flying mode - no collision (Entity.java line 728-733)
        setPos(position_ + movement);
        horizontalCollision_ = false;
        verticalCollision_ = false;
        verticalCollisionBelow_ = false;
        minorHorizontalCollision_ = false;
        this->onGround_ = false;
        return;
    }

    // Call our private collide() method (Entity.java line 754)
    glm::dvec3 actualMovement = collide(movement, level);

    // Update position (Entity.java line 769)
    setPos(position_ + actualMovement);

    // Update collision flags (Entity.java line 774-787)
    bool xBlocked = !MathHelper::approximatelyEquals(movement.x, actualMovement.x);
    bool zBlocked = !MathHelper::approximatelyEquals(movement.z, actualMovement.z);
    horizontalCollision_ = xBlocked || zBlocked;

    // Vertical collision (Entity.java line 777-781)
    // Minecraft: if (Math.abs(delta.y) > 0.0 || this.isLocalInstanceAuthoritative())
    // We always update (equivalent to isLocalInstanceAuthoritative() being true)
    verticalCollision_ = (movement.y != actualMovement.y);
    verticalCollisionBelow_ = verticalCollision_ && movement.y < 0.0;

    // setOnGroundWithMovement (Entity.java line 780)
    setOnGroundWithMovement(verticalCollisionBelow_, horizontalCollision_, actualMovement);

    // minorHorizontalCollision check (Entity.java line 783-787)
    if (horizontalCollision_) {
        // For now, always set to false (full implementation checks collision amount)
        minorHorizontalCollision_ = false;
    } else {
        minorHorizontalCollision_ = false;
    }

    // Reset velocity on collision (Entity.java line 799-800, 805-806)
    glm::dvec3 vel = getVelocity();
    if (horizontalCollision_) {
        setVelocity(xBlocked ? 0.0 : vel.x, vel.y, zBlocked ? 0.0 : vel.z);
    }

    // Reset Y velocity on vertical collision (Entity.java line 805-806)
    // Block.updateEntityMovementAfterFallOn multiplies velocity by (1.0, 0.0, 1.0)
    if (movement.y != actualMovement.y) {
        vel = getVelocity();
        setVelocity(vel.x, 0.0, vel.z);
    }
}

// Check if entity is colliding with a specific block (Entity.java line 339)
// Minecraft: VoxelShape var3 = state.getCollisionShape(this.level(), pos, CollisionContext.of(this)).move((Vec3i)pos);
// Minecraft: return Shapes.joinIsNotEmpty(var3, Shapes.create(this.getBoundingBox()), BooleanOp.AND);
bool Entity::isColliding(const glm::ivec3& blockPos, const BlockState& blockState) const {
    // Fast path: air blocks have no collision
    if (blockState.isAir()) {
        return false;
    }

    // Get the block and its collision shape
    Block* block = BlockRegistry::getBlock(blockState);
    if (!block) {
        return false;
    }

    // Get the block's collision shape (in 0-1 block space)
    // Minecraft: state.getCollisionShape(this.level(), pos, CollisionContext.of(this))
    BlockShape collisionShape = block->getCollisionShape(blockState);

    // Create VoxelShape and move to block position
    // Minecraft: .move((Vec3i)pos)
    auto var3 = VoxelShapes::fromBlockShape(collisionShape, blockPos.x, blockPos.y, blockPos.z);

    // Create VoxelShape from entity bounding box
    // Minecraft: Shapes.create(this.getBoundingBox())
    auto entityShape = Shapes::create(getBoundingBox());

    // Check if intersection is non-empty
    // Minecraft: return Shapes.joinIsNotEmpty(var3, Shapes.create(this.getBoundingBox()), BooleanOp.AND);
    return Shapes::joinIsNotEmpty(var3, entityShape, BooleanOp::AND);
}

// Transform movement input to velocity based on entity rotation (Entity.java line 1605)
glm::dvec3 Entity::movementInputToVelocity(const glm::dvec3& movementInput, float speed, float yawDegrees) {
    double lengthSquared = glm::dot(movementInput, movementInput);

    // Early exit for zero movement (Entity.java line 1607)
    if (lengthSquared < 1.0E-7) {
        return glm::dvec3(0.0);
    }

    // Normalize if length > 1.0 (Entity.java line 1610)
    glm::dvec3 normalized = (lengthSquared > 1.0) ? glm::normalize(movementInput) : movementInput;
    glm::dvec3 scaled = normalized * static_cast<double>(speed);

    // Rotate by yaw angle (Entity.java line 1611-1613)
    float yawRadians = yawDegrees * (glm::pi<float>() / 180.0f);
    float sinYaw = std::sin(yawRadians);
    float cosYaw = std::cos(yawRadians);

    return glm::dvec3(
        scaled.x * cosYaw - scaled.z * sinYaw,  // Rotated X
        scaled.y,                                 // Y unchanged
        scaled.z * cosYaw + scaled.x * sinYaw   // Rotated Z
    );
}

// Static collision methods (Entity.java lines 1137-1180)

// Collide bounding box with world (Entity.java line 1137)
glm::dvec3 Entity::collideBoundingBox(const Entity* source, const glm::dvec3& movement, const AABB& boundingBox,
                                      Level* level, const std::vector<std::shared_ptr<VoxelShape>>& entityColliders) {
    // Minecraft: List var5 = collectColliders(source, level, entityColliders, boundingBox.expandTowards(movement));
    // Minecraft: return collideWithShapes(movement, boundingBox, var5);

    auto colliders = collectColliders(source, level, entityColliders, boundingBox.expandTowards(movement));
    return collideWithShapes(movement, boundingBox, colliders);
}

// Collect all colliders in bounding box (Entity.java line 1142)
std::vector<std::shared_ptr<VoxelShape>> Entity::collectAllColliders(const Entity* source, Level* level, const AABB& boundingBox) {
    // Minecraft: List var3 = level.getEntityCollisions(source, boundingBox);
    // Minecraft: return collectColliders(source, level, var3, boundingBox);

    auto entityCollisions = level->getEntityCollisions(source, boundingBox);
    return collectColliders(source, level, entityCollisions, boundingBox);
}

// Collect colliders (Entity.java line 1147)
std::vector<std::shared_ptr<VoxelShape>> Entity::collectColliders(const Entity* source, Level* level,
                                                                   const std::vector<std::shared_ptr<VoxelShape>>& entityColliders,
                                                                   const AABB& boundingBox) {
    // Minecraft: ImmutableList.Builder var4 = ImmutableList.builderWithExpectedSize(entityColliders.size() + 1);
    std::vector<std::shared_ptr<VoxelShape>> colliders;
    colliders.reserve(entityColliders.size() + 1);

    // Minecraft: if (!entityColliders.isEmpty()) var4.addAll(entityColliders);
    if (!entityColliders.empty()) {
        colliders.insert(colliders.end(), entityColliders.begin(), entityColliders.end());
    }

    // Minecraft: WorldBorder var5 = level.getWorldBorder();
    // Minecraft: boolean var6 = source != null && var5.isInsideCloseToBorder(source, boundingBox);
    // Minecraft: if (var6) var4.add(var5.getCollisionShape());
    // TODO: Add world border collision when WorldBorder is implemented

    // Minecraft: var4.addAll(level.getBlockCollisions(source, boundingBox));
    auto blockCollisions = level->getBlockCollisions(source, boundingBox);
   colliders.insert(colliders.end(), blockCollisions.begin(), blockCollisions.end());

    // Minecraft: return var4.build();
    return colliders;
}

// Collect candidate step up heights (Entity.java line 1110)
std::vector<float> Entity::collectCandidateStepUpHeights(const AABB& boundingBox,
                                                          const std::vector<std::shared_ptr<VoxelShape>>& colliders,
                                                          float maxStepHeight, float stepHeightToSkip) {
    // Minecraft: FloatArraySet var4 = new FloatArraySet(4);
    std::vector<float> heights;
    heights.reserve(4);

    // Minecraft: Iterator var5 = colliders.iterator();
    for (const auto& shape : colliders) {
        // TODO: Implement when VoxelShape has getCoords(Axis.Y)
        // Minecraft: DoubleList var7 = var6.getCoords(Direction.Axis.Y);
        // For each Y coordinate:
        //   float var11 = (float)(var9 - boundingBox.minY);
        //   if (!(var11 < 0.0F) && var11 != stepHeightToSkip) {
        //     if (var11 > maxStepHeight) break;
        //     var4.add(var11);
        //   }
    }

    // Minecraft: float[] var12 = var4.toFloatArray();
    // Minecraft: FloatArrays.unstableSort(var12);
    // TODO: Sort the heights array

    return heights;  // Placeholder
}

// Collide with shapes (Entity.java line 1163)
glm::dvec3 Entity::collideWithShapes(const glm::dvec3& movement, const AABB& boundingBox,
                                     const std::vector<std::shared_ptr<VoxelShape>>& shapes) {
    // Minecraft line 1164: if (shapes.isEmpty()) return movement;
    if (shapes.empty()) {
        return movement;
    } else {
        // Minecraft line 1167: Vec3 var3 = Vec3.ZERO;
        glm::dvec3 var3(0.0, 0.0, 0.0);

        // Minecraft line 1168: UnmodifiableIterator var4 = Direction.axisStepOrder(movement).iterator();
        std::vector<Direction::Axis> axisOrder = Direction::axisStepOrder(movement);

        // Minecraft line 1170: while(var4.hasNext())
        for (Direction::Axis var5 : axisOrder) {
            // Minecraft line 1171: Direction.Axis var5 = (Direction.Axis)var4.next();
            // Minecraft line 1172: double var6 = movement.get(var5);
            double var6 = Direction::choose(var5, movement.x, movement.y, movement.z);

            // Minecraft line 1173: if (var6 != 0.0)
            if (var6 != 0.0) {
                // Minecraft line 1174: double var8 = Shapes.collide(var5, boundingBox.move(var3), shapes, var6);
                double var8 = VoxelShapes::collide(var5, boundingBox.move(var3.x, var3.y, var3.z), shapes, var6);

                // Minecraft line 1175: var3 = var3.with(var5, var8);
                // Vec3.with() returns new Vec3 with the specified axis component replaced
                if (var5 == Direction::Axis::X) {
                    var3.x = var8;
                } else if (var5 == Direction::Axis::Y) {
                    var3.y = var8;
                } else { // Direction::Axis::Z
                    var3.z = var8;
                }
            }
        }

        // Minecraft line 1179: return var3;
        return var3;
    }
}

} // namespace FarHorizon