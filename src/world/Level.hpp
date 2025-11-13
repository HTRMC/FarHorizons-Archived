#pragma once

#include "../physics/AABB.hpp"
#include "../voxel/VoxelShape.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace FarHorizon {

class Entity;

// Level interface (from Minecraft)
// Represents the game world
class Level {
public:
    virtual ~Level() = default;

    // Get entity collisions in bounding box (Minecraft: List<VoxelShape> getEntityCollisions(Entity source, AABB box))
    virtual std::vector<std::shared_ptr<VoxelShape>> getEntityCollisions(const Entity* source, const AABB& box) = 0;

    // Get block collisions in bounding box (Minecraft: Iterable<VoxelShape> getBlockCollisions(Entity source, AABB box))
    virtual std::vector<std::shared_ptr<VoxelShape>> getBlockCollisions(const Entity* source, const AABB& box) = 0;

    // TODO: Add other methods as needed:
    // - getBlockState(BlockPos pos)
    // - setBlockState(BlockPos pos, BlockState state)
    // - getWorldBorder()
    // - getChunkForCollisions(int x, int z)
};

} // namespace FarHorizon