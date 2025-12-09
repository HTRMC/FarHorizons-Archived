#pragma once

#include "AABB.hpp"
#include "voxel/VoxelShapes.hpp"
#include "world/ChunkManager.hpp"
#include "world/BlockState.hpp"
#include "world/BlockRegistry.hpp"
#include "world/BlockShape.hpp"
#include "util/MathHelper.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <algorithm>
#include <memory>
#include <spdlog/spdlog.h>

namespace FarHorizon {

// Minecraft-style collision detection and resolution system
class CollisionSystem {
private:
    ChunkManager* chunkManager_;

public:
    CollisionSystem(ChunkManager* chunkManager)
        : chunkManager_(chunkManager) {}

    // Main collision method matching Minecraft's Entity.collide() (Entity.java line 1076)
    // This is the method that Entity.move() calls
    glm::dvec3 collide(const AABB& entityBox, const glm::dvec3& movement,
                       float stepHeight, bool onGround) const {
        // Get all collisions in the swept volume (Entity.java line 1078)
        AABB sweptBox = entityBox.stretch(movement);
        auto entityCollisions = getBlockCollisions(sweptBox);

        // First try basic collision resolution (Entity.java line 1079)
        glm::dvec3 resolvedMovement = (glm::length(movement) == 0.0)
            ? movement
            : collideBoundingBox(entityBox, movement, entityCollisions);

        // Check if movement was blocked
        bool xBlocked = !MathHelper::approximatelyEquals(movement.x, resolvedMovement.x);
        bool yBlocked = (movement.y != resolvedMovement.y);
        bool zBlocked = !MathHelper::approximatelyEquals(movement.z, resolvedMovement.z);
        bool fallingAndHitGround = yBlocked && movement.y < 0.0;

        // Minecraft's stepping condition (Entity.java line 1084):
        // maxUpStep() > 0.0F && (var8 || this.onGround()) && (var5 || var7)
        // where var8 = fallingAndHitGround, var5 = xBlocked, var7 = zBlocked
        if (stepHeight > AABB::EPSILON &&
            (fallingAndHitGround || onGround) &&
            (xBlocked || zBlocked)) {

            // Try stepping (Entity.java line 1085-1104)
            glm::dvec3 steppedMovement = tryStepUp(
                entityBox, movement, resolvedMovement,
                stepHeight, fallingAndHitGround, entityCollisions
            );

            // Use stepped movement if it's better horizontally (Entity.java line 1100)
            double resolvedHorizontalDist = resolvedMovement.x * resolvedMovement.x +
                                            resolvedMovement.z * resolvedMovement.z;
            double steppedHorizontalDist = steppedMovement.x * steppedMovement.x +
                                           steppedMovement.z * steppedMovement.z;

            if (steppedHorizontalDist > resolvedHorizontalDist) {
                return steppedMovement;
            }
        }

        return resolvedMovement;
    }

    // Basic collision resolution without stepping (Entity.java line 1137: collideBoundingBox)
    glm::dvec3 collideBoundingBox(const AABB& entityBox, const glm::dvec3& movement,
                                  const std::vector<std::shared_ptr<VoxelShape>>& entityCollisions) const {
        // Get all colliders (both entity and block collisions)
        auto allCollisions = collectAllColliders(entityBox.stretch(movement), entityCollisions);

        // Perform axis-by-axis collision resolution (Entity.java line 1139)
        return collideWithShapes(movement, entityBox, allCollisions);
    }

    // Get all block collision shapes in a region
    // Based on Minecraft's BlockCollisionSpliterator
    // Get collision shape for a specific block (used by Entity.isColliding)
    // Matches Minecraft's: state.getCollisionShape(level, pos, CollisionContext.of(this)).move(pos)
    std::shared_ptr<VoxelShape> getBlockCollisionShape(const BlockState& blockState,
                                                        const glm::ivec3& blockPos) const {
        if (blockState.getId() == 0) {
            return nullptr;  // Air block, no collision
        }

        // Get the block's collision shape
        const Block* block = BlockRegistry::getBlock(blockState.getId());
        if (!block) {
            return nullptr;
        }

        // Get the shape and offset it to the block's position (.move(pos) in Minecraft)
        auto shape = VoxelShapes::getBlockShape(block);
        if (shape && !shape->isEmpty()) {
            return shape->offset(blockPos.x, blockPos.y, blockPos.z);
        }

        return nullptr;
    }

    std::vector<std::shared_ptr<VoxelShape>> getBlockCollisions(const AABB& region) const {
        std::vector<std::shared_ptr<VoxelShape>> collisions;

        // Get integer bounds of the region
        int minX = static_cast<int>(std::floor(region.minX));
        int minY = static_cast<int>(std::floor(region.minY));
        int minZ = static_cast<int>(std::floor(region.minZ));
        int maxX = static_cast<int>(std::floor(region.maxX));
        int maxY = static_cast<int>(std::floor(region.maxY));
        int maxZ = static_cast<int>(std::floor(region.maxZ));

        static int debugCounter = 0;
        bool shouldDebug = (++debugCounter % 20 == 0); // Debug every 20 calls

        if (shouldDebug) {
            spdlog::info("getBlockCollisions: region ({:.3f},{:.3f},{:.3f}) to ({:.3f},{:.3f},{:.3f})",
                region.minX, region.minY, region.minZ, region.maxX, region.maxY, region.maxZ);
            spdlog::info("  Checking blocks: ({},{},{}) to ({},{},{})", minX, minY, minZ, maxX, maxY, maxZ);
        }

        int blocksChecked = 0;
        int blocksFound = 0;

        // Iterate through all blocks in the region
        for (int x = minX; x <= maxX; ++x) {
            for (int y = minY; y <= maxY; ++y) {
                for (int z = minZ; z <= maxZ; ++z) {
                    blocksChecked++;

                    // Get block state at position directly
                    BlockState blockState = chunkManager_->getBlockState(glm::ivec3(x, y, z));
                    if (blockState.isAir()) continue; // Air block

                    // Get collision shape at world coordinates (BlockCollisionSpliterator.java line 88)
                    auto shape = getBlockCollisionShape(blockState, x, y, z);

                    if (!shape || shape->isEmpty()) {
                        if (shouldDebug && blocksFound < 3) {
                            spdlog::warn("  Block ({},{},{}): state {} has empty collision shape", x, y, z, blockState.id);
                        }
                        continue;
                    }

                    blocksFound++;
                    if (shouldDebug && blocksFound <= 3) {
                        spdlog::info("  Block ({},{},{}): state {} has collision shape", x, y, z, blockState.id);
                    }

                    collisions.push_back(shape);
                }
            }
        }

        if (shouldDebug) {
            spdlog::info("  Found {} collision shapes out of {} blocks checked", blocksFound, blocksChecked);
        }

        return collisions;
    }

    // Core collision resolution algorithm
    // Based on Minecraft's Entity.adjustMovementForCollisions()
    // Returns the actual movement vector after collision resolution
    glm::dvec3 adjustMovementForCollisions(const AABB& entityBox, const glm::dvec3& movement) const {
        // No movement, no collision
        if (glm::length(movement) < AABB::EPSILON) {
            return glm::dvec3(0.0);
        }

        // Get all collisions in the swept volume
        AABB sweptBox = entityBox.stretch(movement);
        auto collisions = getBlockCollisions(sweptBox);

        if (collisions.empty()) {
            return movement;
        }

        // Perform axis-by-axis collision resolution
        return adjustMovementForCollisionsInternal(entityBox, movement, collisions);
    }

    // Axis-by-axis collision resolution with stepping
    // Based on Minecraft's collision resolution algorithm
    glm::dvec3 adjustMovementForCollisionsWithStepping(const AABB& entityBox, const glm::dvec3& movement,
                                                        float stepHeight) const {
        // First try normal collision resolution
        glm::dvec3 resolvedMovement = adjustMovementForCollisions(entityBox, movement);

        // Check if movement was blocked (Minecraft uses approximatelyEquals)
        bool xBlocked = std::abs(movement.x - resolvedMovement.x) > AABB::EPSILON;
        bool yBlocked = std::abs(movement.y - resolvedMovement.y) > AABB::EPSILON;
        bool zBlocked = std::abs(movement.z - resolvedMovement.z) > AABB::EPSILON;

        // Minecraft's stepping condition (line 1036 in Entity.java):
        // - stepHeight > 0
        // - AND (yBlocked && movement.y < 0) [falling and hit ground]
        // - AND (xBlocked OR zBlocked) [horizontal movement was blocked]
        // CRITICAL: Only step if there's ACTUAL horizontal movement attempt!
        bool hasHorizontalMovement = std::abs(movement.x) > AABB::EPSILON || std::abs(movement.z) > AABB::EPSILON;
        bool fallingAndHitGround = yBlocked && movement.y < 0.0;
        bool horizontalBlocked = xBlocked || zBlocked;

        // DEBUG: Log stepping checks
        if (fallingAndHitGround) {
            spdlog::info("STEP CHECK: mov=({:.6f},{:.6f},{:.6f}) res=({:.6f},{:.6f},{:.6f}) xBlk={} zBlk={} hasHoriz={}",
                movement.x, movement.y, movement.z,
                resolvedMovement.x, resolvedMovement.y, resolvedMovement.z,
                xBlocked, zBlocked, hasHorizontalMovement);
        }

        if (stepHeight > AABB::EPSILON && fallingAndHitGround && horizontalBlocked && hasHorizontalMovement) {
            spdlog::info("STEPPING ACTIVATED!");
            // Try stepping up
            glm::dvec3 steppedMovement = tryStepUp(entityBox, movement, resolvedMovement, stepHeight);

            // Use stepped movement if it's better horizontally
            double resolvedHorizontalDist = resolvedMovement.x * resolvedMovement.x +
                                            resolvedMovement.z * resolvedMovement.z;
            double steppedHorizontalDist = steppedMovement.x * steppedMovement.x +
                                           steppedMovement.z * steppedMovement.z;

            if (steppedHorizontalDist > resolvedHorizontalDist) {
                spdlog::debug("Stepping: using stepped movement Y={:.3f}", steppedMovement.y);
                return steppedMovement;
            }
        }

        return resolvedMovement;
    }

private:
    // Internal axis-by-axis collision resolution
    // Based on Minecraft's static adjustMovementForCollisions method
    glm::dvec3 adjustMovementForCollisionsInternal(const AABB& entityBox, const glm::dvec3& movement,
                                                    const std::vector<std::shared_ptr<VoxelShape>>& collisions) const {
        glm::dvec3 result(0.0);

        // Determine axis processing order based on movement magnitude
        // Process largest movement first for stability
        std::array<int, 3> axisOrder = getAxisOrder(movement);

        AABB currentBox = entityBox;

        for (int axis : axisOrder) {
            double movementOnAxis = getAxisComponent(movement, axis);

            if (std::abs(movementOnAxis) < AABB::EPSILON) {
                continue;
            }

            // Convert axis int to Direction::Axis enum
            Direction::Axis dirAxis = static_cast<Direction::Axis>(axis);

            // Calculate max offset on this axis (Shapes.java line 184: collide)
            double offset = VoxelShapes::collide(dirAxis, currentBox, collisions, movementOnAxis);

            // Apply offset and update current box
            setAxisComponent(result, axis, offset);
            currentBox = currentBox.offset(getAxisVector(axis, offset));
        }

        return result;
    }

    // Minecraft's step-up logic (Entity.java line 1085-1104)
    glm::dvec3 tryStepUp(const AABB& entityBox, const glm::dvec3& originalMovement,
                        const glm::dvec3& blockedMovement, float maxStepHeight,
                        bool fallingAndHitGround,
                        const std::vector<std::shared_ptr<VoxelShape>>& entityCollisions) const {

        // Create starting box (Entity.java line 1085)
        // If falling and hit ground, start from position after Y movement
        // Otherwise start from original position
        AABB stepBox = fallingAndHitGround
            ? entityBox.offset(0.0, blockedMovement.y, 0.0)
            : entityBox;

        // Expand box for step attempt (Entity.java line 1086-1088)
        AABB stepSweptBox = stepBox.expand(originalMovement.x, maxStepHeight, originalMovement.z);
        if (!fallingAndHitGround) {
            stepSweptBox = stepSweptBox.expand(0.0, -9.999999747378752E-6, 0.0);
        }

        // Get all colliders for stepping (Entity.java line 1091)
        auto stepCollisions = collectAllColliders(stepSweptBox, entityCollisions);

        // Collect candidate step heights (Entity.java line 1093)
        float currentY = static_cast<float>(blockedMovement.y);
        std::vector<float> candidateHeights = collectCandidateStepUpHeights(
            stepBox, stepCollisions, maxStepHeight, currentY
        );

        // Try each step height (Entity.java line 1097-1104)
        glm::dvec3 bestMovement = blockedMovement;
        for (float tryHeight : candidateHeights) {
            glm::dvec3 tryMovement(originalMovement.x, tryHeight, originalMovement.z);
            glm::dvec3 result = collideWithShapes(tryMovement, stepBox, stepCollisions);

            // Check if this gives better horizontal movement (Entity.java line 1100)
            if (result.x * result.x + result.z * result.z >
                bestMovement.x * bestMovement.x + bestMovement.z * bestMovement.z) {

                // Adjust for box offset (Entity.java line 1102)
                double yOffset = stepBox.minY - entityBox.minY;
                bestMovement = result - glm::dvec3(0.0, yOffset, 0.0);
            }
        }

        return bestMovement;
    }

    // Old step-up for backwards compatibility
    glm::dvec3 tryStepUp(const AABB& entityBox, const glm::dvec3& originalMovement,
                        const glm::dvec3& blockedMovement, float stepHeight) const {
        auto entityCollisions = getBlockCollisions(entityBox.stretch(originalMovement));
        return tryStepUp(entityBox, originalMovement, blockedMovement, stepHeight,
                        true, entityCollisions);
    }

    // Collect candidate step-up heights (Entity.java line 1110)
    std::vector<float> collectCandidateStepUpHeights(
        const AABB& box,
        const std::vector<std::shared_ptr<VoxelShape>>& collisions,
        float maxStepHeight,
        float stepHeightToSkip) const {

        std::set<float> heights;

        for (const auto& shape : collisions) {
            if (!shape || shape->isEmpty()) continue;

            // Get Y coordinates from shape (Entity.java line 1116)
            const auto& yCoords = shape->getPointPositions(Direction::Axis::Y);
            for (double y : yCoords) {
                float height = static_cast<float>(y - box.minY);

                // Skip if negative or matches current Y (Entity.java line 1122-1123)
                if (height < 0.0f || height == stepHeightToSkip) continue;

                // Stop if exceeds max (Entity.java line 1123)
                if (height > maxStepHeight) break;

                heights.insert(height);
            }
        }

        return std::vector<float>(heights.begin(), heights.end());
    }

    // Collect all colliders (Entity.java line 1147)
    std::vector<std::shared_ptr<VoxelShape>> collectAllColliders(
        const AABB& box,
        const std::vector<std::shared_ptr<VoxelShape>>& entityCollisions) const {

        std::vector<std::shared_ptr<VoxelShape>> result;
        result.reserve(entityCollisions.size() + 16); // Reserve for entity + block collisions

        // Add entity collisions
        result.insert(result.end(), entityCollisions.begin(), entityCollisions.end());

        // Add block collisions (Entity.java line 1159)
        auto blockCollisions = getBlockCollisions(box);
        result.insert(result.end(), blockCollisions.begin(), blockCollisions.end());

        return result;
    }

    // Collide with shapes (Entity.java line 1163: collideWithShapes)
    glm::dvec3 collideWithShapes(const glm::dvec3& movement, const AABB& box,
                                 const std::vector<std::shared_ptr<VoxelShape>>& shapes) const {
        if (shapes.empty()) {
            return movement;
        }

        // Process axes in order of movement magnitude (Entity.java line 1168)
        std::array<int, 3> axisOrder = getAxisOrder(movement);

        glm::dvec3 result(0.0);
        AABB currentBox = box;

        for (int axis : axisOrder) {
            double movementOnAxis = getAxisComponent(movement, axis);
            if (movementOnAxis == 0.0) continue;

            // Collide on this axis (Entity.java line 1174)
            Direction::Axis dirAxis = static_cast<Direction::Axis>(axis);
            double offset = VoxelShapes::collide(dirAxis, currentBox, shapes, movementOnAxis);

            setAxisComponent(result, axis, offset);
            currentBox = currentBox.offset(getAxisVector(axis, offset));
        }

        return result;
    }

    // Collect possible step heights from collision shapes
    // Based on Minecraft's collectStepHeights method
    std::vector<float> collectStepHeights(const AABB& box, const std::vector<std::shared_ptr<VoxelShape>>& collisions,
                                         float maxStepHeight) const {
        std::set<float> stepHeights;
        stepHeights.insert(0.0f); // Always try ground level
        stepHeights.insert(maxStepHeight); // Always try max step height

        // Extract Y-coordinates from collision shapes
        for (const auto& shape : collisions) {
            if (!shape || shape->isEmpty()) continue;

            // Get Y points from the shape
            const auto& yPoints = shape->getPointPositions(Direction::Axis::Y);
            for (double y : yPoints) {
                float stepHeight = static_cast<float>(y - box.minY);
                if (stepHeight >= 0.0f && stepHeight <= maxStepHeight) {
                    stepHeights.insert(stepHeight);
                }
            }
        }

        return std::vector<float>(stepHeights.begin(), stepHeights.end());
    }

    // Get collision shape for a block at world coordinates
    // Based on Block.getCollisionShape (Block.java)
    std::shared_ptr<VoxelShape> getBlockCollisionShape(BlockState blockState, int x, int y, int z) const {
        // Fast path: air blocks have no collision
        if (blockState.isAir()) {
            return VoxelShapes::empty();
        }

        // Get the block and ask for its collision shape
        // This respects each block's custom collision shape (slabs, stairs, etc.)
        Block* block = BlockRegistry::getBlock(blockState);
        if (!block) {
            // Fallback: if block not found, return full cube (shouldn't happen)
            return VoxelShapes::cuboid(x, y, z, x + 1.0, y + 1.0, z + 1.0);
        }

        // Get the block's collision shape (in 0-1 block space)
        BlockShape collisionShape = block->getCollisionShape(blockState);

        // Convert BlockShape to VoxelShape at world position
        return VoxelShapes::fromBlockShape(collisionShape, x, y, z);
    }

    // Helper: Get axis processing order (largest movement first)
    std::array<int, 3> getAxisOrder(const glm::dvec3& movement) const {
        std::array<std::pair<int, double>, 3> axes = {
            std::make_pair(0, std::abs(movement.x)),
            std::make_pair(1, std::abs(movement.y)),
            std::make_pair(2, std::abs(movement.z))
        };

        std::sort(axes.begin(), axes.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });

        return {axes[0].first, axes[1].first, axes[2].first};
    }

    // Helper: Get component of vector on axis
    double getAxisComponent(const glm::dvec3& vec, int axis) const {
        return axis == 0 ? vec.x : (axis == 1 ? vec.y : vec.z);
    }

    // Helper: Set component of vector on axis
    void setAxisComponent(glm::dvec3& vec, int axis, double value) const {
        if (axis == 0) vec.x = value;
        else if (axis == 1) vec.y = value;
        else vec.z = value;
    }

    // Helper: Create vector with value on one axis
    glm::dvec3 getAxisVector(int axis, double value) const {
        if (axis == 0) return glm::dvec3(value, 0, 0);
        if (axis == 1) return glm::dvec3(0, value, 0);
        return glm::dvec3(0, 0, value);
    }
};

} // namespace FarHorizon