#pragma once

#include <glm/glm.hpp>

namespace FarHorizon {

struct BlockState;

// BlockGetter interface (from Minecraft)
// Provides access to block states in the world
class BlockGetter {
public:
    virtual ~BlockGetter() = default;

    // Get block state at position (Minecraft: BlockState getBlockState(BlockPos pos))
    virtual BlockState getBlockState(const glm::ivec3& pos) const = 0;

    // TODO: Add other methods as needed:
    // - getFluidState(BlockPos pos)
    // - getBlockEntity(BlockPos pos)
    // - getLightEmission(BlockPos pos)
};

} // namespace FarHorizon