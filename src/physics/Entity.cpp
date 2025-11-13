#include "Entity.hpp"
#include <cmath>
#include <algorithm>
#include <glm/ext/scalar_constants.hpp>
#include <spdlog/spdlog.h>

#include "world/Level.hpp"

namespace FarHorizon {

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
    baseTick();
}

void Entity::baseTick() {
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
    // In Minecraft: return this.level().noCollision(this, box) && !this.level().containsAnyLiquid(box);
    // We don't have Level, liquids, or noCollision yet
    // For now, return true as placeholder
    return true;  // TODO: Implement when Level system is added
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
        // Get bounding box slightly below entity (Entity.java: AABB var4 = new AABB(var3.minX, var3.minY - 1.0E-6, ...))
        AABB entityBox = getBoundingBox();
        AABB checkBox(
            entityBox.minX, entityBox.minY - 1.0E-6, entityBox.minZ,
            entityBox.maxX, entityBox.minY, entityBox.maxZ
        );

        // In Minecraft: Optional var5 = this.level.findSupportingBlock(this, var4);
        // We don't have Level.findSupportingBlock() yet
        // Full implementation would:
        // 1. Find the supporting block using level.findSupportingBlock(this, checkBox)
        // 2. If not found and !onGroundNoBlocks and movement != null:
        //    - Try again with checkBox moved by -movement
        // 3. Set mainSupportingBlockPos to the result
        // 4. Set onGroundNoBlocks = result.isEmpty()

        // Placeholder implementation
        mainSupportingBlockPos_ = std::nullopt;  // Would be set by findSupportingBlock
        onGroundNoBlocks_ = true;  // Would be set based on findSupportingBlock result

        // TODO: Implement when Level.findSupportingBlock() is available
    } else {
        onGroundNoBlocks_ = false;
        if (mainSupportingBlockPos_.has_value()) {
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
    if (std::abs(movement.y) > 0.0) {
        verticalCollision_ = (movement.y != actualMovement.y);
        verticalCollisionBelow_ = verticalCollision_ && movement.y < 0.0;

        // setOnGroundWithMovement (simplified - Entity.java line 780)
        this->onGround_ = verticalCollisionBelow_;
    }
    // If movement.y == 0, keep previous collision flags

    // minorHorizontalCollision check (Entity.java line 783-787)
    if (horizontalCollision_) {
        // For now, always set to false (full implementation checks collision amount)
        minorHorizontalCollision_ = false;
    } else {
        minorHorizontalCollision_ = false;
    }

    // Reset velocity on collision (Entity.java line 799-800)
    glm::dvec3 vel = getVelocity();
    if (horizontalCollision_) {
        setVelocity(xBlocked ? 0.0 : vel.x, vel.y, zBlocked ? 0.0 : vel.z);
    }
}

// Check if entity is colliding with a specific block (Entity.java line 339)
// Minecraft: state.getCollisionShape(level, pos, CollisionContext.of(this)).move(pos)
bool Entity::isColliding(const glm::ivec3& blockPos, const BlockState& blockState, Level* level) const {
    // Get entity's current bounding box (Entity.java: Shapes.create(this.getBoundingBox()))
    AABB entityBox = getBoundingBox();

    // Get the block's collision shape (Entity.java line 340)
    // state.getCollisionShape(level, pos, CollisionContext.of(this)).move(pos)
    // TODO: Implement when BlockState has getCollisionShape() method
    // auto blockShape = blockState.getCollisionShape(level, blockPos, CollisionContext::of(this));

    // TODO: Move shape to block position
    // blockShape = blockShape->move(blockPos);

    // TODO: Check intersection
    // Shapes.joinIsNotEmpty(var3, Shapes.create(this.getBoundingBox()), BooleanOp.AND)
    // return Shapes::joinIsNotEmpty(blockShape, Shapes::create(entityBox), BooleanOp::AND);

    return false;  // Placeholder
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
    // Minecraft: if (shapes.isEmpty()) return movement;
    if (shapes.empty()) {
        return movement;
    }

    // Minecraft: Vec3 var3 = Vec3.ZERO;
    glm::dvec3 result(0.0, 0.0, 0.0);

    // Minecraft: UnmodifiableIterator var4 = Direction.axisStepOrder(movement).iterator();
    // TODO: Implement axisStepOrder - for now use fixed order Y, X, Z
    // This determines which axis to resolve collisions in first

    // For each axis in order:
    // Minecraft: Direction.Axis var5 = (Direction.Axis)var4.next();
    // Minecraft: double var6 = movement.get(var5);
    // Minecraft: if (var6 != 0.0) {
    //   double var8 = Shapes.collide(var5, boundingBox.move(var3), shapes, var6);
    //   var3 = var3.with(var5, var8);
    // }

    // TODO: Implement Shapes.collide() for each axis
    // For now, return movement as placeholder
    return movement;
}

} // namespace FarHorizon