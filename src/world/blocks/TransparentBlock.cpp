#include "TransparentBlock.hpp"
#include "../BlockRegistry.hpp"

namespace FarHorizon {

bool TransparentBlock::isSideInvisible(BlockState currentState, BlockState neighborState, FaceDirection direction) const {
    // Get the neighbor block
    Block* neighborBlock = BlockRegistry::getBlock(neighborState);

    // If the neighbor is the same block type (glass-to-glass), cull the face
    if (neighborBlock == this) {
        return true;  // Cull internal glass faces
    }

    // Otherwise, use base behavior (don't cull)
    return Block::isSideInvisible(currentState, neighborState, direction);
}

} // namespace FarHorizon
