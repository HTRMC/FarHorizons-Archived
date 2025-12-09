#pragma once

#include <glm/glm.hpp>
#include <optional>
#include "../world/BlockRegistry.hpp"
#include "../world/ChunkManager.hpp"
#include "../world/Block.hpp"
#include "../world/blocks/StairBlock.hpp"
#include "../world/blocks/enums/BlockHalf.hpp"
#include "../world/blocks/enums/StairFacing.hpp"
#include "../world/blocks/enums/StairShape.hpp"
#include "../core/Raycast.hpp"
#include "../audio/AudioManager.hpp"

namespace FarHorizon {

/**
 * Handles block interactions (breaking, placing).
 * Inspired by Minecraft's ClientPlayerInteractionManager.
 */
class InteractionManager {
public:
    InteractionManager(ChunkManager& chunkManager, AudioManager& audioManager);

    /**
     * Attempt to break the block at the given hit result.
     * @return true if block was successfully broken
     */
    bool breakBlock(const BlockHitResult& hitResult);

    /**
     * Attempt to place a block at the given hit result.
     * @param hitResult The raycast hit result
     * @param block The block to place
     * @param cameraForward Camera's forward vector (for orientation)
     * @return true if block was successfully placed
     */
    bool placeBlock(const BlockHitResult& hitResult, Block* block, const glm::vec3& cameraForward);

private:
    ChunkManager& chunkManager_;
    AudioManager& audioManager_;

    /**
     * Calculate the appropriate BlockState for placing a stair block.
     * Handles facing, half, and shape calculations.
     */
    BlockState calculateStairPlacement(
        StairBlock* stairBlock,
        const BlockHitResult& hitResult,
        const glm::vec3& cameraForward,
        const glm::ivec3& worldPos
    );

    /**
     * Calculate stair facing from camera's horizontal direction.
     */
    StairFacing calculateStairFacing(const glm::vec3& forward);

    /**
     * Calculate stair half (top/bottom) based on hit position.
     */
    BlockHalf calculateStairHalf(const BlockHitResult& hitResult);

    /**
     * Calculate stair shape (straight/inner/outer) based on neighbors.
     */
    StairShape calculateStairShape(
        StairFacing facing,
        BlockHalf half,
        const glm::ivec3& worldPos
    );

    /**
     * Helper functions to get directional offsets based on stair facing.
     */
    glm::ivec3 getLeftOffset(StairFacing facing);
    glm::ivec3 getRightOffset(StairFacing facing);
    glm::ivec3 getFrontOffset(StairFacing facing);
    glm::ivec3 getBackOffset(StairFacing facing);

    /**
     * Queue remesh for chunk and neighbors if block is on chunk boundary.
     */
    void queueRemeshIfNeeded(const ChunkPosition& chunkPos, const glm::ivec3& localPos);
};

} // namespace FarHorizon