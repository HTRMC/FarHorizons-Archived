#pragma once

#include "BlockShape.hpp"
#include "BlockState.hpp"
#include "FaceUtils.hpp"
#include "BlockModel.hpp"
#include "Chunk.hpp"
#include <unordered_map>
#include <list>
#include <cstdint>
#include <optional>

namespace FarHorizon {

// LRU cache for geometric face culling comparisons
// Thread-local to avoid contention
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
    std::unordered_map<ShapePair, std::list<std::pair<ShapePair, bool>>::iterator> map_;
    std::list<std::pair<ShapePair, bool>> list_;  // Most recent at front
};

// Central face culling system
class FaceCullingSystem {
public:
    FaceCullingSystem() = default;

    // Main culling function - determines if a face should be drawn
    // Uses 4 fast paths + geometric cache for performance
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
    // Cached per BlockState ID for performance - call precacheAllShapes() after model loading
    const BlockShape& getBlockShape(BlockState state, const BlockModel* model);

    // Pre-compute shapes for all BlockStates (call after block models are loaded)
    // Takes a map of BlockState ID -> BlockModel* to eagerly compute all shapes
    void precacheAllShapes(const std::unordered_map<uint16_t, const BlockModel*>& stateToModel);

    // Clear all caches (useful for testing or after large changes)
    void clearCache();

private:
    // Geometric comparison using voxel-level matching
    // Uses ONLY_FIRST predicate (tests if shape1 has any voxel that shape2 doesn't)
    // Returns true if face should be drawn (NOT culled)
    bool geometricComparison(
        const std::shared_ptr<VoxelSet>& ourFace,
        const std::shared_ptr<VoxelSet>& neighborFace
    );

    // Thread-local cache (one per thread)
    static thread_local FaceCullCache s_cache;

    // BlockShape cache: maps BlockState ID → BlockShape
    // Avoids recomputing shapes every frame (HUGE performance win)
    std::unordered_map<uint16_t, BlockShape> shapeCache_;
};

} // namespace FarHorizon