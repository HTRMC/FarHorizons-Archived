#include "BlockCollisions.hpp"
#include "Entity.hpp"
#include "../voxel/VoxelShape.hpp"
#include "../voxel/Shapes.hpp"
#include "../voxel/BooleanOp.hpp"
#include "CollisionContext.hpp"
#include "BlockGetter.hpp"
#include <cmath>

namespace FarHorizon {

// Constructor (Minecraft: BlockCollisions(CollisionGetter getter, Entity entity, AABB box, boolean onlySuffocatingBlocks))
BlockCollisions::BlockCollisions(CollisionGetter* collisionGetter, const Entity* source, const AABB& box, bool onlySuffocatingBlocks)
    : BlockCollisions(collisionGetter,
                     source == nullptr ? CollisionContext::empty() : CollisionContext::of(source),
                     box, onlySuffocatingBlocks)
{
}

// Constructor with CollisionContext (Minecraft: BlockCollisions(CollisionGetter getter, CollisionContext context, AABB box, boolean onlySuffocatingBlocks))
BlockCollisions::BlockCollisions(CollisionGetter* collisionGetter, CollisionContext* context, const AABB& box, bool onlySuffocatingBlocks)
    : context_(context)
    , pos_(0, 0, 0)
    , collisionGetter_(collisionGetter)
    , box_(box)
    , onlySuffocatingBlocks_(onlySuffocatingBlocks)
    , cachedBlockGetter_(nullptr)
    , cachedBlockGetterPos_(0)
    , cursor_(
        static_cast<int>(std::floor(box.minX - 1.0E-7)) - 1,
        static_cast<int>(std::floor(box.minY - 1.0E-7)) - 1,
        static_cast<int>(std::floor(box.minZ - 1.0E-7)) - 1,
        static_cast<int>(std::floor(box.maxX + 1.0E-7)) + 1,
        static_cast<int>(std::floor(box.maxY + 1.0E-7)) + 1,
        static_cast<int>(std::floor(box.maxZ + 1.0E-7)) + 1
    )
{
    // Create entity shape from AABB (Minecraft: this.entityShape = Shapes.create(box))
    entityShape_ = Shapes::create(box);
}

// Get chunk at position (Minecraft: private BlockGetter getChunk(int x, int z))
BlockGetter* BlockCollisions::getChunk(int x, int z) {
    // TODO: Implement chunk caching with SectionPos and ChunkPos
    // For now, just return the CollisionGetter as BlockGetter
    // Full implementation would:
    // 1. Convert x, z to section coordinates (SectionPos::blockToSectionCoord)
    // 2. Calculate ChunkPos::asLong(sectionX, sectionZ)
    // 3. Check if cached (cachedBlockGetterPos == chunkPos)
    // 4. If not cached, call collisionGetter_->getChunkForCollisions(sectionX, sectionZ)
    // 5. Update cache

    return nullptr;  // Placeholder
}

// Compute the next colliding block (Minecraft: protected BlockPos computeNext())
std::optional<glm::ivec3> BlockCollisions::computeNext() {
    while (cursor_.advance()) {
        int x = cursor_.nextX();
        int y = cursor_.nextY();
        int z = cursor_.nextZ();
        int type = cursor_.getNextType();

        // Skip if type == 3 (Minecraft: if (var4 == 3) continue;)
        if (type == 3) {
            continue;
        }

        // Get chunk (Minecraft: BlockGetter var5 = this.getChunk(var1, var3))
        BlockGetter* chunk = getChunk(x, z);
        if (chunk == nullptr) {
            continue;
        }

        // Set mutable position (Minecraft: this.pos.set(var1, var2, var3))
        pos_ = glm::ivec3(x, y, z);

        // Get block state (Minecraft: BlockState var6 = var5.getBlockState(this.pos))
        BlockState blockState = chunk->getBlockState(pos_);

        // Check suffocating/largeCollisionShape/movingPiston conditions
        // (Minecraft: if (this.onlySuffocatingBlocks && !var6.isSuffocating(var5, this.pos) || var4 == 1 && !var6.hasLargeCollisionShape() || var4 == 2 && !var6.is(Blocks.MOVING_PISTON)))
        // TODO: Implement these checks when BlockState has the methods
        // For now, skip these checks

        // Get collision shape (Minecraft: VoxelShape var7 = this.context.getCollisionShape(var6, this.collisionGetter, this.pos))
        auto shape = context_->getCollisionShape(blockState, collisionGetter_, pos_);

        // Fast path for full block shape (Minecraft: if (var7 == Shapes.block()))
        if (shape == Shapes::block()) {
            // Check AABB intersection (Minecraft: if (!this.box.intersects(...)))
            if (!box_.intersects(static_cast<double>(x), static_cast<double>(y), static_cast<double>(z),
                                 static_cast<double>(x) + 1.0, static_cast<double>(y) + 1.0, static_cast<double>(z) + 1.0)) {
                continue;
            }

            // Return block position (Minecraft: return this.resultProvider.apply(this.pos, var7.move(this.pos)))
            return pos_;
        }

        // Move shape to block position (Minecraft: VoxelShape var8 = var7.move(this.pos))
        auto movedShape = shape->move(pos_);

        // Check if shape is empty or doesn't intersect (Minecraft: if (var8.isEmpty() || !Shapes.joinIsNotEmpty(var8, this.entityShape, BooleanOp.AND)))
        if (movedShape->isEmpty() || !Shapes::joinIsNotEmpty(movedShape, entityShape_, BooleanOp::AND)) {
            continue;
        }

        // Return block position (Minecraft: return this.resultProvider.apply(this.pos, var8))
        return pos_;
    }

    // No more blocks (Minecraft: return this.endOfData())
    return std::nullopt;
}

} // namespace FarHorizon