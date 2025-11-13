#pragma once

#include "AABB.hpp"
#include "CollisionGetter.hpp"
#include "AbstractIterator.hpp"
#include "Cursor3D.hpp"
#include <glm/glm.hpp>
#include <functional>
#include <optional>
#include <memory>

namespace FarHorizon {

class Entity;
class VoxelShape;
class CollisionContext;
class BlockGetter;

// BlockCollisions iterator (from Minecraft)
// Iterates through block positions that collide with an AABB
class BlockCollisions : public AbstractIterator<glm::ivec3> {
public:
    // Constructor (Minecraft: BlockCollisions(CollisionGetter getter, Entity entity, AABB box, boolean onlySuffocatingBlocks))
    BlockCollisions(CollisionGetter* collisionGetter, const Entity* source, const AABB& box, bool onlySuffocatingBlocks);

    // Constructor with CollisionContext (Minecraft: BlockCollisions(CollisionGetter getter, CollisionContext context, AABB box, boolean onlySuffocatingBlocks))
    BlockCollisions(CollisionGetter* collisionGetter, CollisionContext* context, const AABB& box, bool onlySuffocatingBlocks);

protected:
    // Compute the next colliding block position (Minecraft: protected BlockPos computeNext())
    std::optional<glm::ivec3> computeNext() override;

private:
    AABB box_;
    CollisionContext* context_;
    Cursor3D cursor_;
    glm::ivec3 pos_;  // MutableBlockPos in Java
    std::shared_ptr<VoxelShape> entityShape_;
    CollisionGetter* collisionGetter_;
    bool onlySuffocatingBlocks_;
    BlockGetter* cachedBlockGetter_;
    int64_t cachedBlockGetterPos_;

    // Get chunk at position (Minecraft: private BlockGetter getChunk(int x, int z))
    BlockGetter* getChunk(int x, int z);
};

} // namespace FarHorizon