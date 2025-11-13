#pragma once

#include "AABB.hpp"
#include "BlockGetter.hpp"
#include <glm/glm.hpp>
#include <optional>

#include "Entity.hpp"
#include "world/BlockState.hpp"

namespace FarHorizon {

// CollisionGetter interface (from Minecraft)
// Represents an object that can query block collisions in the world
// In Minecraft, CollisionGetter extends BlockGetter
class CollisionGetter : public BlockGetter {
public:
    virtual ~CollisionGetter() = default;

    // Find the supporting block for an entity within a bounding box
    // (Minecraft: default Optional<BlockPos> findSupportingBlock(Entity source, AABB box))
    virtual std::optional<glm::ivec3> findSupportingBlock(const Entity* source, const AABB& box) const;

    // Get chunk for collision queries (Minecraft: @Nullable BlockGetter getChunkForCollisions(int x, int z))
    // Returns a BlockGetter for the chunk at the given chunk coordinates
    // Used by BlockCollisions iterator for efficient chunk access
    virtual BlockGetter* getChunkForCollisions(int chunkX, int chunkZ) = 0;
};

} // namespace FarHorizon