#pragma once

#include "BlockShape.hpp"
#include "BlockState.hpp"
#include "FaceUtils.hpp"
#include <unordered_map>
#include <list>
#include <cstdint>
#include <optional>

namespace VoxelEngine {

// Forward declarations
class BlockModel;
class Chunk;
struct ChunkPosition;

// LRU cache for geometric face culling comparisons
// Thread-local to avoid contention (like Minecraft's FACE_CULL_MAP)
class FaceCullCache {
public:
    static constexpr size_t MAX_SIZE = 256;

    // Get cached result, or nullopt if not in cache
    std::optional<bool> get(const ShapePair& key);

    // Store result in cache (evicts oldest entry if full)
    void put(const ShapePair& key, bool shouldCull);

    // Clear the entire cache
    void clear();

private:
    // LRU implementation using unordered_map + list
    // Key: ShapePair, Value: iterator to list element
    std::unordered_map<ShapePair, std::list<std::pair<ShapePair, bool>>::iterator> m_map;
    std::list<std::pair<ShapePair, bool>> m_list;  // Most recent at front
};

// Central face culling system
// Implements Minecraft's shouldDrawSide() logic adapted for VulkanVoxel
class FaceCullingSystem {
public:
    FaceCullingSystem() = default;

    // Main culling function - determines if a face should be drawn
    // Implements Minecraft's Block.shouldDrawSide() with 4 fast paths + geometric cache
    //
    // currentState: The block we're rendering
    // neighborState: The adjacent block in the given direction
    // face: The direction of the face we're considering
    // currentShape: Geometry of current block
    // neighborShape: Geometry of neighbor block
    //
    // Returns: true if face should be drawn, false if it should be culled
    bool shouldDrawFace(
        BlockState currentState,
        BlockState neighborState,
        FaceDirection face,
        const BlockShape& currentShape,
        const BlockShape& neighborShape
    );

    // Safe neighbor block access with chunk boundary checking
    // Inspired by ChunkRegion.getBlockState() and getChunk()
    //
    // currentChunk: The chunk we're meshing
    // currentChunkPos: Position of currentChunk
    // localX/Y/Z: Block position within chunk (may be out of bounds)
    // getChunkFunc: Function to retrieve neighbor chunks (returns nullptr if not loaded)
    //
    // Returns: BlockState of neighbor, or AIR if neighbor chunk not loaded
    BlockState getNeighborBlockState(
        const Chunk* currentChunk,
        const ChunkPosition& currentChunkPos,
        int localX, int localY, int localZ,
        const std::function<const Chunk*(const ChunkPosition&)>& getChunkFunc
    ) const;

    // Get the culling shape for a block (maps BlockModel to BlockShape)
    // Cached per BlockState ID for performance - call rebuildShapeCache() after model changes
    const BlockShape& getBlockShape(BlockState state, const BlockModel* model);

    // Rebuild the shape cache (call after block models are loaded/changed)
    void rebuildShapeCache(const std::function<const BlockModel*(uint16_t)>& getModelFunc);

    // Clear all caches (useful for testing or after large changes)
    void clearCache();

private:
    // Geometric comparison for partial shapes
    // Checks if our face is completely covered by neighbor's geometry
    // Returns true if face should be drawn (NOT culled)
    bool geometricComparison(
        const BlockShape& ourShape,
        const BlockShape& neighborShape,
        FaceDirection face
    );

    // Thread-local cache (one per thread, like Minecraft)
    static thread_local FaceCullCache s_cache;

    // BlockShape cache: maps BlockState ID → BlockShape
    // Avoids recomputing shapes every frame (HUGE performance win)
    std::unordered_map<uint16_t, BlockShape> m_shapeCache;
};

} // namespace VoxelEngine