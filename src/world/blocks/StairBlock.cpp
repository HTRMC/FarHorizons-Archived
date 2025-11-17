#include "StairBlock.hpp"
#include "../BlockRegistry.hpp"

namespace FarHorizon {

// Helper: Check if stair can take shape in a direction (Minecraft StairBlock.java line 156-159)
bool StairBlock::canTakeShape(BlockState state, const BlockGetter& level, const glm::ivec3& pos, StairFacing neighbour) {
    // Get the block in the neighbor direction (Java: pos.relative(neighbour))
    glm::ivec3 neighbourPos = pos + HorizontalDirection::getOffset(neighbour);
    BlockState neighbourState = level.getBlockState(neighbourPos);

    // canTakeShape returns true if:
    // 1. Neighbor is NOT a stair, OR
    // 2. Neighbor stair has different facing, OR
    // 3. Neighbor stair has different half
    if (!isStairs(neighbourState)) {
        return true;
    }

    Block* neighbourBlock = BlockRegistry::getBlock(neighbourState);
    StairBlock* neighbourStair = static_cast<StairBlock*>(neighbourBlock);
    StairFacing neighbourFacing = neighbourStair->getFacing(neighbourState);
    BlockHalf neighbourHalf = neighbourStair->getHalf(neighbourState);

    StairBlock* currentStair = static_cast<StairBlock*>(BlockRegistry::getBlock(state));
    StairFacing currentFacing = currentStair->getFacing(state);
    BlockHalf currentHalf = currentStair->getHalf(state);

    return neighbourFacing != currentFacing || neighbourHalf != currentHalf;
}

// Helper: Check if a block state is a stair (Minecraft StairBlock.java line 161-163)
bool StairBlock::isStairs(BlockState state) {
    Block* block = BlockRegistry::getBlock(state);
    return dynamic_cast<StairBlock*>(block) != nullptr;
}

// Get stair shape based on neighbors (Minecraft StairBlock.java line 127-154)
StairShape StairBlock::getStairsShape(BlockState state, const BlockGetter& level, const glm::ivec3& pos) {
    // Get current stair's facing direction
    StairBlock* currentStair = static_cast<StairBlock*>(BlockRegistry::getBlock(state));
    StairFacing facing = currentStair->getFacing(state);
    BlockHalf half = currentStair->getHalf(state);

    // Check block in FRONT (in the direction the stairs are facing)
    glm::ivec3 frontPos = pos + HorizontalDirection::getOffset(facing);
    BlockState frontState = level.getBlockState(frontPos);

    if (isStairs(frontState)) {
        StairBlock* frontStair = static_cast<StairBlock*>(BlockRegistry::getBlock(frontState));
        BlockHalf frontHalf = frontStair->getHalf(frontState);

        // Only connect if same half
        if (half == frontHalf) {
            StairFacing frontFacing = frontStair->getFacing(frontState);

            // Check if perpendicular (different axis) and can take shape
            if (HorizontalDirection::getAxis(frontFacing) != HorizontalDirection::getAxis(facing) &&
                canTakeShape(state, level, pos, HorizontalDirection::getOpposite(frontFacing))) {

                // Check if it's counter-clockwise
                if (frontFacing == HorizontalDirection::getCounterClockWise(facing)) {
                    return StairShape::OUTER_LEFT;
                }
                return StairShape::OUTER_RIGHT;
            }
        }
    }

    // Check block in BACK (opposite to facing direction)
    glm::ivec3 backPos = pos + HorizontalDirection::getOffset(HorizontalDirection::getOpposite(facing));
    BlockState backState = level.getBlockState(backPos);

    if (isStairs(backState)) {
        StairBlock* backStair = static_cast<StairBlock*>(BlockRegistry::getBlock(backState));
        BlockHalf backHalf = backStair->getHalf(backState);

        // Only connect if same half
        if (half == backHalf) {
            StairFacing backFacing = backStair->getFacing(backState);

            // Check if perpendicular (different axis) and can take shape
            if (HorizontalDirection::getAxis(backFacing) != HorizontalDirection::getAxis(facing) &&
                canTakeShape(state, level, pos, backFacing)) {

                // Check if it's counter-clockwise
                if (backFacing == HorizontalDirection::getCounterClockWise(facing)) {
                    return StairShape::INNER_LEFT;
                }
                return StairShape::INNER_RIGHT;
            }
        }
    }

    return StairShape::STRAIGHT;
}

} // namespace FarHorizon