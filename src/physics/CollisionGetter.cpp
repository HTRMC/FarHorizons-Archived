#include "CollisionGetter.hpp"
#include "Entity.hpp"
#include <cmath>
#include <limits>

namespace FarHorizon {

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

    // Get block range to check from the AABB
    int minX = static_cast<int>(std::floor(box.minX));
    int minY = static_cast<int>(std::floor(box.minY));
    int minZ = static_cast<int>(std::floor(box.minZ));
    int maxX = static_cast<int>(std::floor(box.maxX));
    int maxY = static_cast<int>(std::floor(box.maxY));
    int maxZ = static_cast<int>(std::floor(box.maxZ));

    // TODO: Implement BlockCollisions iterator (Minecraft's approach)
    // For now, return empty as placeholder
    // Full implementation would:
    // 1. Create BlockCollisions iterator: new BlockCollisions(this, source, box, false, (var0, var1) -> { return var0; })
    // 2. Iterate through colliding blocks with var6.hasNext() and var6.next()
    // 3. Calculate distance for each colliding block
    // 4. Return the closest block position

    return std::nullopt;  // Placeholder
}

} // namespace FarHorizon