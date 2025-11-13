#pragma once

#include "../physics/AABB.hpp"
#include "../physics/CollisionGetter.hpp"
#include "../voxel/VoxelShape.hpp"
#include "../voxel/VoxelShapes.hpp"
#include "ChunkManager.hpp"
#include "BlockState.hpp"
#include "BlockRegistry.hpp"
#include "BlockShape.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace FarHorizon {

class Entity;

// Level class (from Minecraft's Level.java)
// Represents the game world and handles collision detection
class Level : public CollisionGetter {
private:
    ChunkManager* chunkManager_;

public:
    Level(ChunkManager* chunkManager)
        : chunkManager_(chunkManager) {}

    virtual ~Level() = default;

    // Get entity collisions in bounding box (Minecraft: List<VoxelShape> getEntityCollisions(Entity source, AABB box))
    std::vector<std::shared_ptr<VoxelShape>> getEntityCollisions(const Entity* source, const AABB& box) {
        // TODO: Implement entity-entity collision when we have multiple entities
        // For now, return empty since we only have one player
        return std::vector<std::shared_ptr<VoxelShape>>();
    }

    // Get block collisions in bounding box (Minecraft: Iterable<VoxelShape> getBlockCollisions(Entity source, AABB box))
    std::vector<std::shared_ptr<VoxelShape>> getBlockCollisions(const Entity* source, const AABB& box) {
        std::vector<std::shared_ptr<VoxelShape>> collisions;

        // Get integer bounds of the region
        int minX = static_cast<int>(std::floor(box.minX));
        int minY = static_cast<int>(std::floor(box.minY));
        int minZ = static_cast<int>(std::floor(box.minZ));
        int maxX = static_cast<int>(std::floor(box.maxX));
        int maxY = static_cast<int>(std::floor(box.maxY));
        int maxZ = static_cast<int>(std::floor(box.maxZ));

        // Iterate through all blocks in the region
        for (int x = minX; x <= maxX; ++x) {
            for (int y = minY; y <= maxY; ++y) {
                for (int z = minZ; z <= maxZ; ++z) {
                    // Get block state at position via chunk
                    ChunkPosition chunkPos = chunkManager_->worldToChunkPos(glm::vec3(x, y, z));
                    Chunk* chunk = chunkManager_->getChunk(chunkPos);
                    if (!chunk) continue;

                    // Convert world coords to chunk-local coords
                    int localX = x - chunkPos.x * 16;
                    int localY = y - chunkPos.y * 16;
                    int localZ = z - chunkPos.z * 16;

                    // Clamp to valid chunk bounds
                    if (localX < 0 || localX >= 16 || localY < 0 || localY >= 16 || localZ < 0 || localZ >= 16) {
                        continue;
                    }

                    BlockState blockState = chunk->getBlockState(localX, localY, localZ);
                    if (blockState.isAir()) continue; // Air block

                    // Get collision shape at world coordinates
                    auto shape = getBlockCollisionShape(blockState, glm::ivec3(x, y, z));

                    if (!shape || shape->isEmpty()) {
                        continue;
                    }

                    collisions.push_back(shape);
                }
            }
        }

        return collisions;
    }

    // Check if entity has no collision at position (Minecraft: boolean noCollision(Entity entity, AABB box))
    bool noCollision(const Entity* entity, const AABB& box) const {
        // Get all block collisions in the box
        auto blockCollisions = const_cast<Level*>(this)->getBlockCollisions(entity, box);
        if (!blockCollisions.empty()) {
            return false;  // Has block collisions
        }

        // Get all entity collisions in the box
        auto entityCollisions = const_cast<Level*>(this)->getEntityCollisions(entity, box);
        if (!entityCollisions.empty()) {
            return false;  // Has entity collisions
        }

        // TODO: Check world border collision when WorldBorder is implemented

        return true;  // No collisions
    }

    // Check if box contains any liquid (Minecraft: boolean containsAnyLiquid(AABB box))
    bool containsAnyLiquid(const AABB& box) const {
        // TODO: Implement when fluid system is added
        return false;  // No liquids yet
    }

    // CollisionGetter interface: Get block state at a position
    BlockState getBlockState(const glm::ivec3& pos) const override {
        ChunkPosition chunkPos = chunkManager_->worldToChunkPos(glm::vec3(pos.x, pos.y, pos.z));
        Chunk* chunk = chunkManager_->getChunk(chunkPos);
        if (!chunk) {
            return BlockState();  // Return air
        }

        // Convert world coords to chunk-local coords
        int localX = pos.x - chunkPos.x * 16;
        int localY = pos.y - chunkPos.y * 16;
        int localZ = pos.z - chunkPos.z * 16;

        // Clamp to valid chunk bounds
        if (localX < 0 || localX >= 16 || localY < 0 || localY >= 16 || localZ < 0 || localZ >= 16) {
            return BlockState();  // Return air
        }

        return chunk->getBlockState(localX, localY, localZ);
    }

    // Get chunk for collision queries (CollisionGetter: getChunkForCollisions)
    // Returns this Level as a BlockGetter since Level already handles coordinate conversion
    BlockGetter* getChunkForCollisions(int chunkX, int chunkZ) override {
        // In Minecraft, this would return the specific chunk at (chunkX, chunkZ)
        // For simplicity, we return the Level itself which already implements BlockGetter
        // and handles world-to-chunk coordinate conversion in getBlockState()

        // Check if chunk exists at this position
        ChunkPosition pos{chunkX, 0, chunkZ};  // Y doesn't matter for 2D chunk lookup

        // We could optimize by caching, but for now just return this
        // The Level's getBlockState will handle the coordinate conversion
        return this;
    }

private:
    // Get collision shape for a block at world coordinates
    std::shared_ptr<VoxelShape> getBlockCollisionShape(const BlockState& blockState, const glm::ivec3& pos) const {
        // Fast path: air blocks have no collision
        if (blockState.isAir()) {
            return VoxelShapes::empty();
        }

        // Get the block and ask for its collision shape
        Block* block = BlockRegistry::getBlock(blockState);
        if (!block) {
            // Fallback: if block not found, return full cube
            return VoxelShapes::cuboid(pos.x, pos.y, pos.z, pos.x + 1.0, pos.y + 1.0, pos.z + 1.0);
        }

        // Get the block's collision shape (in 0-1 block space)
        BlockShape collisionShape = block->getCollisionShape(blockState);

        // Convert BlockShape to VoxelShape at world position
        return VoxelShapes::fromBlockShape(collisionShape, pos.x, pos.y, pos.z);
    }
};

} // namespace FarHorizon