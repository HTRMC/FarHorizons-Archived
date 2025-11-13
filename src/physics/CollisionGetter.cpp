#include "CollisionGetter.hpp"
#include "BlockCollisions.hpp"
#include "Entity.hpp"
#include <cmath>
#include <limits>

namespace FarHorizon {

// Helper: Calculate squared distance from block center to position (Minecraft: BlockPos.distToCenterSqr)
static double distToCenterSqr(const glm::ivec3& blockPos, const glm::dvec3& position) {
    double dx = static_cast<double>(blockPos.x) + 0.5 - position.x;
    double dy = static_cast<double>(blockPos.y) + 0.5 - position.y;
    double dz = static_cast<double>(blockPos.z) + 0.5 - position.z;
    return dx * dx + dy * dy + dz * dz;
}

// Helper: Compare block positions (Minecraft: BlockPos.compareTo)
// Returns negative if a < b, positive if a > b, zero if equal
static int compareBlockPos(const glm::ivec3& a, const glm::ivec3& b) {
    if (a.y != b.y) return a.y - b.y;
    if (a.z != b.z) return a.z - b.z;
    return a.x - b.x;
}

// Find the supporting block for an entity (Minecraft: default Optional<BlockPos> findSupportingBlock)
std::optional<glm::ivec3> CollisionGetter::findSupportingBlock(const Entity* source, const AABB& box) const {
    // Minecraft implementation:
    // BlockPos var3 = null;
    // double var4 = Double.MAX_VALUE;
    // BlockCollisions var6 = new BlockCollisions(this, source, box, false, (var0, var1) -> { return var0; });
    // while(true) {
    //   BlockPos var7;
    //   double var8;
    //   do {
    //     if (!var6.hasNext()) {
    //       return Optional.ofNullable(var3);
    //     }
    //     var7 = (BlockPos)var6.next();
    //     var8 = var7.distToCenterSqr(source.position());
    //   } while(!(var8 < var4) && (var8 != var4 || var3 != null && var3.compareTo(var7) >= 0));
    //   var3 = var7.immutable();
    //   var4 = var8;
    // }

    glm::ivec3 closestBlock;
    double closestDistSq = std::numeric_limits<double>::max();
    bool foundBlock = false;

    // Create BlockCollisions iterator (Minecraft: new BlockCollisions(this, source, box, false, ...))
    // Note: Cast away const since BlockCollisions needs non-const pointer
    BlockCollisions blockCollisions(const_cast<CollisionGetter*>(this), source, box, false);

    // Iterate through all colliding blocks (Minecraft: while(true) { ... if (!var6.hasNext()) return ... })
    while (blockCollisions.hasNext()) {
        glm::ivec3 blockPos = blockCollisions.next();

        // Calculate distance to entity center (Minecraft: var8 = var7.distToCenterSqr(source.position()))
        double distSq = distToCenterSqr(blockPos, source->getPos());

        // Keep if closer, or if same distance use compareTo logic
        // (Minecraft: while(!(var8 < var4) && (var8 != var4 || var3 != null && var3.compareTo(var7) >= 0)))
        // The logic is inverted - we want to update when: distSq < closestDistSq OR (distSq == closestDistSq AND compareTo < 0)
        if (distSq < closestDistSq || (distSq == closestDistSq && foundBlock && compareBlockPos(blockPos, closestBlock) < 0)) {
            closestBlock = blockPos;
            closestDistSq = distSq;
            foundBlock = true;
        }
    }

    if (foundBlock) {
        return closestBlock;
    }
    return std::nullopt;
}

} // namespace FarHorizon