#include "InteractionManager.hpp"
#include <spdlog/spdlog.h>
#include <cmath>

namespace FarHorizon {

InteractionManager::InteractionManager(ChunkManager& chunkManager, AudioManager& audioManager)
    : chunkManager_(chunkManager), audioManager_(audioManager) {}

bool InteractionManager::breakBlock(const BlockHitResult& hitResult) {
    // Check if chunk exists
    ChunkPosition chunkPos = chunkManager_.worldToChunkPos(glm::vec3(hitResult.blockPos));
    if (!chunkManager_.hasChunk(chunkPos)) {
        return false;
    }

    // Get sound group from registry before breaking
    const BlockSoundGroup& soundGroup = BlockRegistry::getSoundGroup(hitResult.state);

    // Set to air using copy-on-write
    chunkManager_.setBlockState(hitResult.blockPos, BlockRegistry::AIR->getDefaultState());

    // Notify neighboring blocks that this block was removed
    chunkManager_.notifyNeighbors(hitResult.blockPos, BlockRegistry::AIR->getDefaultState());

    // Play break sound
    audioManager_.playSoundEvent(soundGroup.getBreakSound(), soundGroup.getVolume(), soundGroup.getPitch());

    return true;
}

bool InteractionManager::placeBlock(const BlockHitResult& hitResult, Block* block, const glm::vec3& cameraForward) {
    // Place block adjacent to the hit face
    glm::ivec3 placePos = hitResult.blockPos + hitResult.normal;
    ChunkPosition chunkPos = chunkManager_.worldToChunkPos(glm::vec3(placePos));

    if (!chunkManager_.hasChunk(chunkPos)) {
        return false;
    }

    // Check if target position is air
    BlockState existingState = chunkManager_.getBlockState(placePos);
    if (!existingState.isAir()) {
        return false;  // Can't place in non-air block
    }

    BlockState placedState = block->getDefaultState();

    // Special placement logic for stairs
    if (dynamic_cast<StairBlock*>(block)) {
        StairBlock* stairBlock = static_cast<StairBlock*>(block);
        placedState = calculateStairPlacement(stairBlock, hitResult, cameraForward, placePos);
    }

    // Place the block using copy-on-write
    chunkManager_.setBlockState(placePos, placedState);

    // Notify neighboring blocks that a new block was placed
    chunkManager_.notifyNeighbors(placePos, placedState);

    // Play place sound
    const BlockSoundGroup& soundGroup = BlockRegistry::getSoundGroup(placedState);
    audioManager_.playSoundEvent(soundGroup.getPlaceSound(), soundGroup.getVolume(), soundGroup.getPitch());

    return true;
}

BlockState InteractionManager::calculateStairPlacement(
    StairBlock* stairBlock,
    const BlockHitResult& hitResult,
    const glm::vec3& cameraForward,
    const glm::ivec3& worldPos
) {
    // Determine facing from player's horizontal direction
    StairFacing facing = calculateStairFacing(cameraForward);

    // Determine half based on where on the block was clicked
    BlockHalf half = calculateStairHalf(hitResult);

    // Create initial state with straight shape
    BlockState initialState = stairBlock->withFacingHalfAndShape(facing, half, StairShape::STRAIGHT);

    // Calculate shape based on neighbors using Minecraft's logic
    StairShape shape = StairBlock::getStairsShape(initialState, chunkManager_, worldPos);

    spdlog::debug("Placing stairs: facing={}, half={}, shape={}, forward=({:.2f}, {:.2f}, {:.2f})",
                 (int)facing, (int)half, (int)shape,
                 cameraForward.x, cameraForward.y, cameraForward.z);

    return stairBlock->withFacingHalfAndShape(facing, half, shape);
}

StairFacing InteractionManager::calculateStairFacing(const glm::vec3& forward) {
    if (abs(forward.x) > abs(forward.z)) {
        return (forward.x > 0) ? StairFacing::EAST : StairFacing::WEST;
    } else {
        return (forward.z > 0) ? StairFacing::SOUTH : StairFacing::NORTH;
    }
}

BlockHalf InteractionManager::calculateStairHalf(const BlockHitResult& hitResult) {
    if (hitResult.normal.y == 1) {
        return BlockHalf::BOTTOM;
    } else if (hitResult.normal.y == -1) {
        return BlockHalf::TOP;
    } else {
        float fractionalY = hitResult.hitPos.y - floor(hitResult.hitPos.y);
        return (fractionalY > 0.5f) ? BlockHalf::TOP : BlockHalf::BOTTOM;
    }
}

StairShape InteractionManager::calculateStairShape(
    StairFacing facing,
    BlockHalf half,
    const glm::ivec3& worldPos
) {
    StairShape shape = StairShape::STRAIGHT;

    // Check left neighbor
    glm::ivec3 leftOffset = getLeftOffset(facing);
    glm::ivec3 leftWorldPos = worldPos + leftOffset;

    BlockState leftState = chunkManager_.getBlockState(leftWorldPos);
    Block* leftBlock = BlockRegistry::getBlock(leftState);

    if (dynamic_cast<StairBlock*>(leftBlock)) {
        StairBlock* leftStair = static_cast<StairBlock*>(leftBlock);

        int offset = leftState.id - leftStair->baseStateId_;
        StairFacing leftFacing = static_cast<StairFacing>(offset % 4);
        BlockHalf leftHalf = static_cast<BlockHalf>((offset / 4) % 2);

        if (leftHalf == half) {
            glm::ivec3 leftFront = getFrontOffset(leftFacing);
            glm::ivec3 ourRight = getRightOffset(facing);
            if (leftFront == ourRight) {
                shape = StairShape::OUTER_LEFT;
            }
            glm::ivec3 leftBack = getBackOffset(leftFacing);
            if (leftBack == ourRight) {
                shape = StairShape::INNER_LEFT;
            }
        }
    }

    // Check right neighbor
    glm::ivec3 rightOffset = getRightOffset(facing);
    glm::ivec3 rightWorldPos = worldPos + rightOffset;

    BlockState rightState = chunkManager_.getBlockState(rightWorldPos);
    Block* rightBlock = BlockRegistry::getBlock(rightState);

    if (dynamic_cast<StairBlock*>(rightBlock)) {
        StairBlock* rightStair = static_cast<StairBlock*>(rightBlock);

        int offset = rightState.id - rightStair->baseStateId_;
        StairFacing rightFacing = static_cast<StairFacing>(offset % 4);
        BlockHalf rightHalf = static_cast<BlockHalf>((offset / 4) % 2);

        if (rightHalf == half) {
            glm::ivec3 rightFront = getFrontOffset(rightFacing);
            glm::ivec3 ourLeft = getLeftOffset(facing);
            if (rightFront == ourLeft) {
                shape = StairShape::OUTER_RIGHT;
            }
            glm::ivec3 rightBack = getBackOffset(rightFacing);
            if (rightBack == ourLeft) {
                shape = StairShape::INNER_RIGHT;
            }
        }
    }

    return shape;
}

glm::ivec3 InteractionManager::getLeftOffset(StairFacing facing) {
    switch (facing) {
        case StairFacing::NORTH: return glm::ivec3(-1, 0, 0);
        case StairFacing::SOUTH: return glm::ivec3(1, 0, 0);
        case StairFacing::WEST:  return glm::ivec3(0, 0, 1);
        case StairFacing::EAST:  return glm::ivec3(0, 0, -1);
    }
    return glm::ivec3(0, 0, 0);
}

glm::ivec3 InteractionManager::getRightOffset(StairFacing facing) {
    switch (facing) {
        case StairFacing::NORTH: return glm::ivec3(1, 0, 0);
        case StairFacing::SOUTH: return glm::ivec3(-1, 0, 0);
        case StairFacing::WEST:  return glm::ivec3(0, 0, -1);
        case StairFacing::EAST:  return glm::ivec3(0, 0, 1);
    }
    return glm::ivec3(0, 0, 0);
}

glm::ivec3 InteractionManager::getFrontOffset(StairFacing facing) {
    switch (facing) {
        case StairFacing::NORTH: return glm::ivec3(0, 0, -1);
        case StairFacing::SOUTH: return glm::ivec3(0, 0, 1);
        case StairFacing::WEST:  return glm::ivec3(-1, 0, 0);
        case StairFacing::EAST:  return glm::ivec3(1, 0, 0);
    }
    return glm::ivec3(0, 0, 0);
}

glm::ivec3 InteractionManager::getBackOffset(StairFacing facing) {
    switch (facing) {
        case StairFacing::NORTH: return glm::ivec3(0, 0, 1);
        case StairFacing::SOUTH: return glm::ivec3(0, 0, -1);
        case StairFacing::WEST:  return glm::ivec3(1, 0, 0);
        case StairFacing::EAST:  return glm::ivec3(-1, 0, 0);
    }
    return glm::ivec3(0, 0, 0);
}

void InteractionManager::queueRemeshIfNeeded(const ChunkPosition& chunkPos, const glm::ivec3& localPos) {
    // No longer needed - setBlockState handles neighbor remeshing automatically
}

} // namespace FarHorizon