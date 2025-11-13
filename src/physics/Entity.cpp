#include "Entity.hpp"
#include <cmath>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace FarHorizon {

void Entity::updateLastRenderPos() {
    lastRenderPos = position;
}
    
// Get yaw rotation (Entity.java: public float getYRot())
float Entity::getYRot() const {
    return yaw;
}

// Get pitch rotation (Entity.java: public float getXRot())
float Entity::getXRot() const {
    return pitch;
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
    setBoundingBox(this.makeBoundingBox());
    // Note: In Minecraft, this also calls setBoundingBox(makeBoundingBox())
    // Our getBoundingBox() is calculated on-demand, so no need to update it here
}

// Create bounding box from current position and dimensions (Entity.java: protected AABB makeBoundingBox())
AABB Entity::makeBoundingBox() const {
    return dimensions_.makeBoundingBox(position);
}

// Reapply position to update bounding box (Entity.java: protected void reapplyPosition())
void Entity::reapplyPosition() {
    lastKnownPosition_ = std::nullopt;
    setPos(position.x, position.y, position.z);
}

// Turn entity based on mouse input (Entity.java: public void turn(double xo, double yo))
void Entity::turn(double xo, double yo) {
    float var5 = static_cast<float>(yo) * 0.15f;
    float var6 = static_cast<float>(xo) * 0.15f;
    setXRot(getXRot() + var5);
    setYRot(getYRot() + var6);
    setXRot(std::clamp(getXRot(), -90.0f, 90.0f));
    lastPitch += var5;
    lastYaw += var6;
    lastPitch = std::clamp(lastPitch, -90.0f, 90.0f);
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
        lastKnownPosition_ = position;
    }

    lastKnownSpeed_ = position - lastKnownPosition_.value();
    lastKnownPosition_ = position;
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
    m_onGround = onGround;
    checkSupportingBlock(onGround, nullptr);
}

// Set on ground with movement (Entity.java: public void setOnGroundWithMovement(boolean onGround, Vec3 movement))
void Entity::setOnGroundWithMovement(bool onGround, const glm::dvec3& movement) {
    setOnGroundWithMovement(onGround, m_horizontalCollision, movement);
}

// Set on ground with movement (3-parameter version)
void Entity::setOnGroundWithMovement(bool onGround, bool horizontalCollision, const glm::dvec3& movement) {
    m_onGround = onGround;
    m_horizontalCollision = horizontalCollision;
    checkSupportingBlock(onGround, &movement);
}

// Check if entity is supported by a specific block position (Entity.java: public boolean isSupportedBy(BlockPos pos))
bool Entity::isSupportedBy(const glm::ivec3& pos) const {
    return m_mainSupportingBlockPos.has_value() && m_mainSupportingBlockPos.value() == pos;
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
        m_mainSupportingBlockPos = std::nullopt;  // Would be set by findSupportingBlock
        m_onGroundNoBlocks = true;  // Would be set based on findSupportingBlock result

        // TODO: Implement when Level.findSupportingBlock() is available
    } else {
        m_onGroundNoBlocks = false;
        if (m_mainSupportingBlockPos.has_value()) {
            m_mainSupportingBlockPos = std::nullopt;
        }
    }
}

// Set yaw rotation (Entity.java: public void setYRot(float yRot))
void Entity::setYRot(float yRot) {
    if (!std::isfinite(yRot)) {
        spdlog::warn("Invalid entity rotation: {}, discarding.", yRot);
    } else {
        this->yaw = yRot;
    }
}

// Set pitch rotation (Entity.java: public void setXRot(float xRot))
void Entity::setXRot(float xRot) {
    if (!std::isfinite(xRot)) {
        spdlog::warn("Invalid entity rotation: {}, discarding.", xRot);
    } else {
        this->pitch = std::clamp(std::fmod(xRot, 360.0f), -90.0f, 90.0f);
    }
}

void Entity::teleport(const glm::dvec3& position) {
    position = position;
    lastRenderPos = position;
    velocity = glm::dvec3(0.0);
    this->m_onGround = false;
}

// Private collision method (Entity.java line 1076)
// This is the core collision logic that handles both basic collision and stepping
glm::dvec3 Entity::collide(const glm::dvec3& movement, CollisionSystem& collisionSystem) {
    AABB entityBox = getBoundingBox();

    // Get entity collisions (for now empty - Entity.java line 1078)
    std::vector<std::shared_ptr<VoxelShape>> entityCollisions;

    // First try basic collision resolution (Entity.java line 1079)
    glm::dvec3 resolvedMovement = (glm::length(movement) == 0.0)
        ? movement
        : collisionSystem.collideBoundingBox(entityBox, movement, entityCollisions);

    // Check if movement was blocked (Entity.java line 1080-1083)
    bool xBlocked = movement.x != resolvedMovement.x;
    bool yBlocked = movement.y != resolvedMovement.y;
    bool zBlocked = movement.z != resolvedMovement.z;
    bool fallingAndHitGround = yBlocked && movement.y < 0.0;

    // Minecraft's stepping condition (Entity.java line 1084):
    // maxUpStep() > 0.0F && (fallingAndHitGround || this.onGround()) && (xBlocked || zBlocked)
    float stepHeight = getStepHeight();
    if (stepHeight > 0.0f &&
        (fallingAndHitGround || this->m_onGround) &&
        (xBlocked || zBlocked)) {

        // Create starting box for step attempt (Entity.java line 1085)
        AABB stepBox = fallingAndHitGround
            ? entityBox.offset(0.0, resolvedMovement.y, 0.0)
            : entityBox;

        // Expand box for step attempt (Entity.java line 1086-1088)
        AABB stepSweptBox = stepBox.expand(movement.x, stepHeight, movement.z);
        if (!fallingAndHitGround) {
            stepSweptBox = stepSweptBox.expand(0.0, -9.999999747378752E-6, 0.0);
        }

        // Get all colliders for stepping (Entity.java line 1091)
        auto stepCollisions = collisionSystem.collectAllColliders(stepSweptBox, entityCollisions);

        // Collect candidate step heights (Entity.java line 1093)
        float currentY = static_cast<float>(resolvedMovement.y);
        std::vector<float> candidateHeights = collisionSystem.collectCandidateStepUpHeights(
            stepBox, stepCollisions, stepHeight, currentY
        );

        // Try each step height (Entity.java line 1097-1104)
        for (float tryHeight : candidateHeights) {
            glm::dvec3 tryMovement(movement.x, tryHeight, movement.z);
            glm::dvec3 result = collisionSystem.collideWithShapes(tryMovement, stepBox, stepCollisions);

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
void Entity::move(MovementType type, const glm::dvec3& movement, CollisionSystem& collisionSystem) {
    if (noClip) {
        // Creative flying mode - no collision (Entity.java line 728-733)
        setPos(position + movement);
        m_horizontalCollision = false;
        verticalCollision = false;
        verticalCollisionBelow = false;
        minorHorizontalCollision = false;
        this->m_onGround = false;
        return;
    }

    // Call our private collide() method (Entity.java line 754)
    glm::dvec3 actualMovement = collide(movement, collisionSystem);

    // Update position (Entity.java line 769)
    setPos(position + actualMovement);

    // Update collision flags (Entity.java line 774-787)
    bool xBlocked = !MathHelper::approximatelyEquals(movement.x, actualMovement.x);
    bool zBlocked = !MathHelper::approximatelyEquals(movement.z, actualMovement.z);
    m_horizontalCollision = xBlocked || zBlocked;

    // Vertical collision (Entity.java line 777-781)
    if (std::abs(movement.y) > 0.0) {
        verticalCollision = (movement.y != actualMovement.y);
        verticalCollisionBelow = verticalCollision && movement.y < 0.0;

        // setOnGroundWithMovement (simplified - Entity.java line 780)
        this->m_onGround = verticalCollisionBelow;
    }
    // If movement.y == 0, keep previous collision flags

    // minorHorizontalCollision check (Entity.java line 783-787)
    if (m_horizontalCollision) {
        // For now, always set to false (full implementation checks collision amount)
        minorHorizontalCollision = false;
    } else {
        minorHorizontalCollision = false;
    }

    // Reset velocity on collision (Entity.java line 799-800)
    glm::dvec3 vel = getVelocity();
    if (m_horizontalCollision) {
        setVelocity(xBlocked ? 0.0 : vel.x, vel.y, zBlocked ? 0.0 : vel.z);
    }
}

// Check if entity is colliding with a specific block (Entity.java line 339)
// Minecraft: state.getCollisionShape(level, pos, CollisionContext.of(this)).move(pos)
bool Entity::isColliding(const glm::ivec3& blockPos, const BlockState& blockState,
                         CollisionSystem& collisionSystem) const {
    // Get entity's current bounding box (Entity.java: Shapes.create(this.getBoundingBox()))
    AABB entityBox = getBoundingBox();

    // Get the block's collision shape (Entity.java line 340)
    // state.getCollisionShape(level, pos, CollisionContext.of(this)).move(pos)
    auto blockShape = collisionSystem.getBlockCollisionShape(blockState, blockPos);

    // If block has no collision shape, no collision
    if (!blockShape || blockShape->isEmpty()) {
        return false;
    }

    // Check intersection (Entity.java line 341)
    // Shapes.joinIsNotEmpty(var3, Shapes.create(this.getBoundingBox()), BooleanOp.AND)
    return blockShape->intersects(entityBox);
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

} // namespace FarHorizon