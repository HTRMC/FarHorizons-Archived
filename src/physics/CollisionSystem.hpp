#pragma once

#include "AABB.hpp"
#include "voxel/VoxelShapes.hpp"
#include "world/ChunkManager.hpp"
#include "world/BlockState.hpp"
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

    // Get all block collision shapes in a region
    // Based on Minecraft's BlockCollisionSpliterator
    std::vector<std::shared_ptr<VoxelShape>> getBlockCollisions(const AABB& region) const {
        std::vector<std::shared_ptr<VoxelShape>> collisions;

        // Get integer bounds of the region
        int minX = static_cast<int>(std::floor(region.minX));
        int minY = static_cast<int>(std::floor(region.minY));
        int minZ = static_cast<int>(std::floor(region.minZ));
        int maxX = static_cast<int>(std::floor(region.maxX));
        int maxY = static_cast<int>(std::floor(region.maxY));
        int maxZ = static_cast<int>(std::floor(region.maxZ));

        // Iterate through all blocks in the region
        for (int x = minX; x <= maxX; ++x) {
            for (int y = minY; y <= maxY; ++y) {
                for (int z = minZ; z <= maxZ; ++z) {
                    // Get block state at position via chunk
                    ChunkPosition chunkPos = chunkManager_->worldToChunkPos(glm::vec3(x, y, z));
                    Chunk* chunk = chunkManager_->getChunk(chunkPos);
                    if (!chunk) continue;

                    // Convert world coords to chunk-local coords
                    int localX = x - chunkPos.x * 16;
                    int localY = y - chunkPos.y * 16;
                    int localZ = z - chunkPos.z * 16;

                    // Clamp to valid chunk bounds
                    if (localX < 0 || localX >= 16 || localY < 0 || localY >= 16 || localZ < 0 || localZ >= 16) {
                        continue;
                    }

                    BlockState blockState = chunk->getBlockState(localX, localY, localZ);
                    if (blockState.isAir()) continue; // Air block

                    // Get collision shape at world coordinates (BlockCollisionSpliterator.java line 88)
                    auto shape = getBlockCollisionShape(blockState, x, y, z);

                    if (!shape || shape->isEmpty()) continue;

                    collisions.push_back(shape);
                }
            }
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

            // Calculate max offset on this axis (VoxelShapes.java line 270)
            double offset = VoxelShapes::calculateMaxOffset(dirAxis, currentBox, collisions, movementOnAxis);

            // Apply offset and update current box
            setAxisComponent(result, axis, offset);
            currentBox = currentBox.offset(getAxisVector(axis, offset));
        }

        return result;
    }

    // Step height implementation
    // Based on Minecraft's step logic in Entity.move()
    glm::dvec3 tryStepUp(const AABB& entityBox, const glm::dvec3& originalMovement,
                        const glm::dvec3& blockedMovement, float stepHeight) const {

        // Create a box for the step attempt
        AABB stepBox = entityBox.offset(0.0, blockedMovement.y, 0.0);

        // Get swept volume for step attempt (horizontal movement + step up)
        glm::dvec3 stepUpMovement(originalMovement.x, stepHeight, originalMovement.z);
        AABB stepSweptBox = stepBox.stretch(stepUpMovement);

        // Get collisions for stepping
        auto stepCollisions = getBlockCollisions(stepSweptBox);

        // Collect all possible step heights from collision edges
        std::vector<float> possibleStepHeights = collectStepHeights(stepBox, stepCollisions, stepHeight);

        // Try each step height and find the best one
        glm::dvec3 bestMovement = blockedMovement;
        double bestHorizontalDist = blockedMovement.x * blockedMovement.x +
                                   blockedMovement.z * blockedMovement.z;

        for (float tryStepHeight : possibleStepHeights) {
            // Try horizontal movement at this step height
            glm::dvec3 tryMovement(originalMovement.x, tryStepHeight, originalMovement.z);
            glm::dvec3 resolvedMovement = adjustMovementForCollisionsInternal(stepBox, tryMovement, stepCollisions);

            // Check if this gives better horizontal movement
            double horizontalDist = resolvedMovement.x * resolvedMovement.x +
                                   resolvedMovement.z * resolvedMovement.z;

            if (horizontalDist > bestHorizontalDist) {
                bestHorizontalDist = horizontalDist;
                // Adjust Y to account for initial box offset
                bestMovement = resolvedMovement;
                bestMovement.y += blockedMovement.y;
            }
        }

        return bestMovement;
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
        // For now, return full cube at world coordinates for all solid blocks
        // TODO: Get actual collision shape from block type (slabs, stairs, etc.)
        if (blockState.isAir()) {
            return VoxelShapes::empty();
        }

        // Create a full cube shape at the block's world position
        // VoxelShapes::cuboid creates shape with absolute world coordinates
        return VoxelShapes::cuboid(x, y, z, x + 1.0, y + 1.0, z + 1.0);
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