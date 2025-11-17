#include "InteractionManager.hpp"
#include <spdlog/spdlog.h>
#include <cmath>

namespace FarHorizon {

InteractionManager::InteractionManager(ChunkManager& chunkManager, AudioManager& audioManager)
    : chunkManager_(chunkManager), audioManager_(audioManager) {}

bool InteractionManager::breakBlock(const BlockHitResult& hitResult) {
    ChunkPosition chunkPos = chunkManager_.worldToChunkPos(glm::vec3(hitResult.blockPos));
    Chunk* chunk = chunkManager_.getChunk(chunkPos);

    if (!chunk) {
        return false;
    }

    glm::ivec3 localPos = hitResult.blockPos - glm::ivec3(
        chunkPos.x * CHUNK_SIZE,
        chunkPos.y * CHUNK_SIZE,
        chunkPos.z * CHUNK_SIZE
    );

    // Get sound group from registry (data-driven, no virtual calls)
    const BlockSoundGroup& soundGroup = BlockRegistry::getSoundGroup(hitResult.state);

    // Set to air
    chunk->setBlockState(localPos.x, localPos.y, localPos.z, BlockRegistry::AIR->getDefaultState());
    chunkManager_.queueChunkRemesh(chunkPos);

    // Play break sound
    audioManager_.playSoundEvent(soundGroup.getBreakSound(), soundGroup.getVolume(), soundGroup.getPitch());

    // Remesh neighbors if on chunk boundary
    queueRemeshIfNeeded(chunkPos, localPos);

    return true;
}

bool InteractionManager::placeBlock(const BlockHitResult& hitResult, Block* block, const glm::vec3& cameraForward) {
    // Place block adjacent to the hit face
    glm::ivec3 placePos = hitResult.blockPos + hitResult.normal;
    ChunkPosition chunkPos = chunkManager_.worldToChunkPos(glm::vec3(placePos));
    Chunk* chunk = chunkManager_.getChunk(chunkPos);

    if (!chunk) {
        return false;
    }

    glm::ivec3 localPos = placePos - glm::ivec3(
        chunkPos.x * CHUNK_SIZE,
        chunkPos.y * CHUNK_SIZE,
        chunkPos.z * CHUNK_SIZE
    );

    // Check bounds
    if (localPos.x < 0 || localPos.x >= CHUNK_SIZE ||
        localPos.y < 0 || localPos.y >= CHUNK_SIZE ||
        localPos.z < 0 || localPos.z >= CHUNK_SIZE) {
        return false;
    }

    BlockState placedState = block->getDefaultState();

    // Special placement logic for stairs
    if (dynamic_cast<StairBlock*>(block)) {
        StairBlock* stairBlock = static_cast<StairBlock*>(block);
        placedState = calculateStairPlacement(stairBlock, hitResult, cameraForward, chunk, localPos, placePos);
    }

    // Place the block
    chunk->setBlockState(localPos.x, localPos.y, localPos.z, placedState);
    chunkManager_.queueChunkRemesh(chunkPos);

    // Play place sound
    const BlockSoundGroup& soundGroup = BlockRegistry::getSoundGroup(placedState);
    audioManager_.playSoundEvent(soundGroup.getPlaceSound(), soundGroup.getVolume(), soundGroup.getPitch());

    // Remesh neighbors if on chunk boundary
    queueRemeshIfNeeded(chunkPos, localPos);

    return true;
}

BlockState InteractionManager::calculateStairPlacement(
    StairBlock* stairBlock,
    const BlockHitResult& hitResult,
    const glm::vec3& cameraForward,
    Chunk* chunk,
    const glm::ivec3& localPos,
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

    // Debug log
    spdlog::debug("Placing stairs: facing={}, half={}, shape={}, forward=({:.2f}, {:.2f}, {:.2f}), hitPos=({:.2f}, {:.2f}, {:.2f}), normal=({}, {}, {})",
                 (int)facing, (int)half, (int)shape,
                 cameraForward.x, cameraForward.y, cameraForward.z,
                 hitResult.hitPos.x, hitResult.hitPos.y, hitResult.hitPos.z,
                 hitResult.normal.x, hitResult.normal.y, hitResult.normal.z);

    return stairBlock->withFacingHalfAndShape(facing, half, shape);
}

StairFacing InteractionManager::calculateStairFacing(const glm::vec3& forward) {
    // Get horizontal direction (ignore Y component)
    if (abs(forward.x) > abs(forward.z)) {
        return (forward.x > 0) ? StairFacing::EAST : StairFacing::WEST;
    } else {
        return (forward.z > 0) ? StairFacing::SOUTH : StairFacing::NORTH;
    }
}

BlockHalf InteractionManager::calculateStairHalf(const BlockHitResult& hitResult) {
    if (hitResult.normal.y == 1) {
        // Clicked top face - place as bottom half
        return BlockHalf::BOTTOM;
    } else if (hitResult.normal.y == -1) {
        // Clicked bottom face - place as top half
        return BlockHalf::TOP;
    } else {
        // Clicked on a vertical face - check Y position
        float fractionalY = hitResult.hitPos.y - floor(hitResult.hitPos.y);
        return (fractionalY > 0.5f) ? BlockHalf::TOP : BlockHalf::BOTTOM;
    }
}

StairShape InteractionManager::calculateStairShape(
    StairFacing facing,
    BlockHalf half,
    Chunk* chunk,
    const glm::ivec3& localPos,
    const glm::ivec3& worldPos
) {
    StairShape shape = StairShape::STRAIGHT;

    // Check left neighbor
    glm::ivec3 leftOffset = getLeftOffset(facing);
    glm::ivec3 leftWorldPos = worldPos + leftOffset;
    glm::ivec3 leftLocalPos = localPos + leftOffset;
    Chunk* leftChunk = chunk;

    // Check if we need to get neighbor chunk
    if (leftLocalPos.x < 0 || leftLocalPos.x >= CHUNK_SIZE ||
        leftLocalPos.y < 0 || leftLocalPos.y >= CHUNK_SIZE ||
        leftLocalPos.z < 0 || leftLocalPos.z >= CHUNK_SIZE) {
        ChunkPosition leftChunkPos = chunkManager_.worldToChunkPos(glm::vec3(leftWorldPos));
        leftChunk = chunkManager_.getChunk(leftChunkPos);
        if (leftChunk) {
            leftLocalPos = glm::ivec3(
                leftWorldPos.x - leftChunkPos.x * CHUNK_SIZE,
                leftWorldPos.y - leftChunkPos.y * CHUNK_SIZE,
                leftWorldPos.z - leftChunkPos.z * CHUNK_SIZE
            );
        }
    }

    if (leftChunk && leftLocalPos.x >= 0 && leftLocalPos.x < CHUNK_SIZE &&
        leftLocalPos.y >= 0 && leftLocalPos.y < CHUNK_SIZE &&
        leftLocalPos.z >= 0 && leftLocalPos.z < CHUNK_SIZE) {

        BlockState leftState = leftChunk->getBlockState(leftLocalPos.x, leftLocalPos.y, leftLocalPos.z);
        Block* leftBlock = BlockRegistry::getBlock(leftState);

        if (dynamic_cast<StairBlock*>(leftBlock)) {
            StairBlock* leftStair = static_cast<StairBlock*>(leftBlock);

            // Decode properties from state ID
            int offset = leftState.id - leftStair->baseStateId_;
            StairFacing leftFacing = static_cast<StairFacing>(offset % 4);
            BlockHalf leftHalf = static_cast<BlockHalf>((offset / 4) % 2);

            // Only connect if same half
            if (leftHalf == half) {
                // Check if left stair is facing towards us (outer corner)
                glm::ivec3 leftFront = getFrontOffset(leftFacing);
                glm::ivec3 ourRight = getRightOffset(facing);
                if (leftFront == ourRight) {
                    shape = StairShape::OUTER_LEFT;
                }
                // Check if left stair is facing away (inner corner)
                glm::ivec3 leftBack = getBackOffset(leftFacing);
                if (leftBack == ourRight) {
                    shape = StairShape::INNER_LEFT;
                }
            }
        }
    }

    // Check right neighbor
    glm::ivec3 rightOffset = getRightOffset(facing);
    glm::ivec3 rightWorldPos = worldPos + rightOffset;
    glm::ivec3 rightLocalPos = localPos + rightOffset;
    Chunk* rightChunk = chunk;

    // Check if we need to get neighbor chunk
    if (rightLocalPos.x < 0 || rightLocalPos.x >= CHUNK_SIZE ||
        rightLocalPos.y < 0 || rightLocalPos.y >= CHUNK_SIZE ||
        rightLocalPos.z < 0 || rightLocalPos.z >= CHUNK_SIZE) {
        ChunkPosition rightChunkPos = chunkManager_.worldToChunkPos(glm::vec3(rightWorldPos));
        rightChunk = chunkManager_.getChunk(rightChunkPos);
        if (rightChunk) {
            rightLocalPos = glm::ivec3(
                rightWorldPos.x - rightChunkPos.x * CHUNK_SIZE,
                rightWorldPos.y - rightChunkPos.y * CHUNK_SIZE,
                rightWorldPos.z - rightChunkPos.z * CHUNK_SIZE
            );
        }
    }

    if (rightChunk && rightLocalPos.x >= 0 && rightLocalPos.x < CHUNK_SIZE &&
        rightLocalPos.y >= 0 && rightLocalPos.y < CHUNK_SIZE &&
        rightLocalPos.z >= 0 && rightLocalPos.z < CHUNK_SIZE) {

        BlockState rightState = rightChunk->getBlockState(rightLocalPos.x, rightLocalPos.y, rightLocalPos.z);
        Block* rightBlock = BlockRegistry::getBlock(rightState);

        if (dynamic_cast<StairBlock*>(rightBlock)) {
            StairBlock* rightStair = static_cast<StairBlock*>(rightBlock);

            // Decode properties from state ID
            int offset = rightState.id - rightStair->baseStateId_;
            StairFacing rightFacing = static_cast<StairFacing>(offset % 4);
            BlockHalf rightHalf = static_cast<BlockHalf>((offset / 4) % 2);

            // Only connect if same half
            if (rightHalf == half) {
                // Check if right stair is facing towards us (outer corner)
                glm::ivec3 rightFront = getFrontOffset(rightFacing);
                glm::ivec3 ourLeft = getLeftOffset(facing);
                if (rightFront == ourLeft) {
                    shape = StairShape::OUTER_RIGHT;
                }
                // Check if right stair is facing away (inner corner)
                glm::ivec3 rightBack = getBackOffset(rightFacing);
                if (rightBack == ourLeft) {
                    shape = StairShape::INNER_RIGHT;
                }
            }
        }
    }

    return shape;
}

glm::ivec3 InteractionManager::getLeftOffset(StairFacing facing) {
    switch (facing) {
        case StairFacing::NORTH: return glm::ivec3(-1, 0, 0);  // West
        case StairFacing::SOUTH: return glm::ivec3(1, 0, 0);   // East
        case StairFacing::WEST:  return glm::ivec3(0, 0, 1);   // South
        case StairFacing::EAST:  return glm::ivec3(0, 0, -1);  // North
    }
    return glm::ivec3(0, 0, 0);
}

glm::ivec3 InteractionManager::getRightOffset(StairFacing facing) {
    switch (facing) {
        case StairFacing::NORTH: return glm::ivec3(1, 0, 0);   // East
        case StairFacing::SOUTH: return glm::ivec3(-1, 0, 0);  // West
        case StairFacing::WEST:  return glm::ivec3(0, 0, -1);  // North
        case StairFacing::EAST:  return glm::ivec3(0, 0, 1);   // South
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
    if (localPos.x == 0 || localPos.x == CHUNK_SIZE - 1 ||
        localPos.y == 0 || localPos.y == CHUNK_SIZE - 1 ||
        localPos.z == 0 || localPos.z == CHUNK_SIZE - 1) {
        chunkManager_.queueNeighborRemesh(chunkPos);
    }
}

} // namespace FarHorizon