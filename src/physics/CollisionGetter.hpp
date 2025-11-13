#pragma once

#include "AABB.hpp"
#include <glm/glm.hpp>
#include <optional>

#include "Entity.hpp"
#include "world/BlockState.hpp"

namespace FarHorizon {

// CollisionGetter interface (from Minecraft)
// Represents an object that can query block collisions in the world
class CollisionGetter {
public:
    virtual ~CollisionGetter() = default;

    // Find the supporting block for an entity within a bounding box
    // (Minecraft: default Optional<BlockPos> findSupportingBlock(Entity source, AABB box))
    virtual std::optional<glm::ivec3> findSupportingBlock(const Entity* source, const AABB& box) const;

    // Get block state at a position (to be implemented by subclasses)
    virtual BlockState getBlockState(const glm::ivec3& pos) const = 0;
};

} // namespace FarHorizon