#include "FaceCullingSystem.hpp"
#include "Chunk.hpp"
#include "BlockModel.hpp"
#include "BlockRegistry.hpp"
#include <spdlog/spdlog.h>

namespace FarHorizon {

// Helper function for voxel-level comparison (ONLY_FIRST predicate)
// Returns true if shape1 has any voxel that shape2 doesn't have
static bool matchesAnywhere(const VoxelSet& shape1, const VoxelSet& shape2) {
    // Early exit: if shape1 is empty, nothing is exposed
    if (shape1.isEmpty()) {
        return false;
    }

    // Early exit: if shape2 is empty, shape1 is fully exposed
    if (shape2.isEmpty()) {
        return true;
    }

    // Get dimensions - use the maximum to cover all voxels
    int maxX = std::max(shape1.getXSize(), shape2.getXSize());
    int maxY = std::max(shape1.getYSize(), shape2.getYSize());
    int maxZ = std::max(shape1.getZSize(), shape2.getZSize());

    // Compare all voxel positions
    // ONLY_FIRST predicate: shape1.contains(voxel) && !shape2.contains(voxel)
    // IMPORTANT: Scale coordinates to each shape's resolution when comparing
    for (int x = 0; x < maxX; x++) {
        for (int y = 0; y < maxY; y++) {
            for (int z = 0; z < maxZ; z++) {
                // Map coordinates from merged grid to each shape's individual grid
                // This handles cases where shapes have different resolutions
                // (e.g., full cube at 1x1x1 vs slab at 1x2x1)
                int x1 = (x * shape1.getXSize()) / maxX;
                int y1 = (y * shape1.getYSize()) / maxY;
                int z1 = (z * shape1.getZSize()) / maxZ;

                int x2 = (x * shape2.getXSize()) / maxX;
                int y2 = (y * shape2.getYSize()) / maxY;
                int z2 = (z * shape2.getZSize()) / maxZ;

                bool inShape1 = shape1.inBoundsAndContains(x1, y1, z1);
                bool inShape2 = shape2.inBoundsAndContains(x2, y2, z2);

                // Found a voxel in shape1 that's not in shape2
                if (inShape1 && !inShape2) {
                    return true;
                }
            }
        }
    }

    return false;
}

// ============================================================================
// FaceCullCache Implementation
// ============================================================================

std::optional<bool> FaceCullCache::get(const ShapePair& key) {
    auto it = map_.find(key);
    if (it != map_.end()) {
        // Move to front (most recently used)
        list_.splice(list_.begin(), list_, it->second);
        return it->second->second;  // Return cached result
    }
    return std::nullopt;  // Cache miss
}

void FaceCullCache::put(const ShapePair& key, bool shouldCull) {
    auto it = map_.find(key);

    if (it != map_.end()) {
        // Update existing entry and move to front
        it->second->second = shouldCull;
        list_.splice(list_.begin(), list_, it->second);
    } else {
        // New entry
        if (map_.size() >= MAX_SIZE) {
            // Evict least recently used (back of list)
            auto last = list_.end();
            --last;
            map_.erase(last->first);
            list_.pop_back();
        }

        // Insert at front
        list_.push_front({key, shouldCull});
        map_[key] = list_.begin();
    }
}

void FaceCullCache::clear() {
    map_.clear();
    list_.clear();
}

// ============================================================================
// FaceCullingSystem Implementation
// ============================================================================

// Thread-local cache instance
thread_local FaceCullCache FaceCullingSystem::s_cache;

bool FaceCullingSystem::shouldDrawFace(
    BlockState currentState,
    BlockState neighborState,
    FaceDirection face,
    const BlockShape& currentShape,
    const BlockShape& neighborShape
) {
    // ========================================================================
    // FAST PATH 1: Neighbor is full cube → cull face
    // ========================================================================
    if (neighborShape.isFullCube()) {
        return false;  // Completely covered, don't draw
    }

    // ========================================================================
    // FAST PATH 2: Special block logic (glass-to-glass, etc.)
    // ========================================================================
    // Implements special culling cases:
    // - Glass blocks adjacent to other glass blocks (cull internal faces)
    // - Could be extended for water-to-water, leaves-to-leaves, etc.
    Block* currentBlock = BlockRegistry::getBlock(currentState);
    if (currentBlock && currentBlock->isSideInvisible(currentState, neighborState, face)) {
        return false;  // Block says this face should be invisible
    }

    // ========================================================================
    // FAST PATH 3: Neighbor is empty/air → draw face
    // ========================================================================
    if (neighborShape.isEmpty() || neighborState.isAir()) {
        return true;  // Nothing blocking, must draw
    }

    // ========================================================================
    // FAST PATH 4: Our face is empty → draw face
    // ========================================================================
    if (currentShape.isEmpty()) {
        return true;  // We have no geometry on this face, shouldn't reach here
    }

    // ========================================================================
    // SLOW PATH: Geometric comparison with caching
    // ========================================================================
    // Algorithm:
    //   1. Extract face-specific geometry from both blocks
    //   2. Check cache for previous result
    //   3. If cache miss, perform voxel-level comparison
    //   4. Cache result for future queries

    // Extract face-specific geometry
    // IMPORTANT: neighbor uses OPPOSITE face direction (adjacent faces touch)
    auto ourFace = currentShape.getCullingFace(face);
    auto neighborFace = neighborShape.getCullingFace(getOpposite(face));

    // Create cache key with face-extracted VoxelSets
    ShapePair pair{ourFace, neighborFace};

    // Check cache
    auto cached = s_cache.get(pair);
    if (cached.has_value()) {
        return cached.value();  // Return cached result
    }

    // Cache miss - perform geometric comparison using voxels
    bool shouldDraw = geometricComparison(ourFace, neighborFace);

    // Store in cache
    s_cache.put(pair, shouldDraw);

    return shouldDraw;
}

BlockState FaceCullingSystem::getNeighborBlockState(
    const Chunk* currentChunk,
    const ChunkPosition& currentChunkPos,
    int localX, int localY, int localZ,
    const std::function<const Chunk*(const ChunkPosition&)>& getChunkFunc
) const {
    // ========================================================================
    // Neighbor block access logic:
    //   1. Convert block position to chunk coordinates
    //   2. Check if chunk is within dependency radius
    //   3. Return block state or AIR if out of range
    // ========================================================================

    // Check if coordinates are within current chunk
    if (localX >= 0 && localX < CHUNK_SIZE &&
        localY >= 0 && localY < CHUNK_SIZE &&
        localZ >= 0 && localZ < CHUNK_SIZE) {
        // Within bounds - use current chunk
        return currentChunk->getBlockState(localX, localY, localZ);
    }

    // Out of bounds - need to access neighbor chunk
    ChunkPosition neighborChunkPos = currentChunkPos;

    // Adjust chunk position and wrap local coordinates
    if (localX < 0) {
        neighborChunkPos.x--;
        localX = CHUNK_SIZE - 1;
    } else if (localX >= CHUNK_SIZE) {
        neighborChunkPos.x++;
        localX = 0;
    }

    if (localY < 0) {
        neighborChunkPos.y--;
        localY = CHUNK_SIZE - 1;
    } else if (localY >= CHUNK_SIZE) {
        neighborChunkPos.y++;
        localY = 0;
    }

    if (localZ < 0) {
        neighborChunkPos.z--;
        localZ = CHUNK_SIZE - 1;
    } else if (localZ >= CHUNK_SIZE) {
        neighborChunkPos.z++;
        localZ = 0;
    }

    // ========================================================================
    // Chunk boundary check:
    //   - Check if neighbor chunk exists
    //   - If not loaded, treat as AIR (causes faces at render edge to draw)
    //   - This ensures correct rendering at render distance boundary
    // ========================================================================

    const Chunk* neighborChunk = getChunkFunc(neighborChunkPos);

    if (neighborChunk) {
        return neighborChunk->getBlockState(localX, localY, localZ);
    }

    // Neighbor chunk not loaded - treat as AIR
    // This is correct behavior: faces at chunk boundaries should be drawn
    // if the neighbor chunk isn't loaded (visible at render distance edge)
    return BlockState(0);  // AIR
}

const BlockShape& FaceCullingSystem::getBlockShape(BlockState state, const BlockModel* model) {
    // Check cache first (O(1) lookup)
    auto it = shapeCache_.find(state.id);
    if (it != shapeCache_.end()) {
        return it->second;
    }

    // Cache miss - compute shape and store it
    // Use Block's getOutlineShape() for culling logic
    if (state.isAir()) {
        shapeCache_[state.id] = BlockShape::empty();
        return shapeCache_[state.id];
    }

    // Get the block and ask for its outline shape
    // This respects each block's custom shape definition (slabs, stairs, etc.)
    Block* block = BlockRegistry::getBlock(state);
    if (block) {
        shapeCache_[state.id] = block->getOutlineShape(state);
    } else {
        // Fallback: compute from model if block not found (shouldn't happen in normal operation)
        if (!model || model->elements.empty()) {
            shapeCache_[state.id] = BlockShape::empty();
        } else {
            // Compute bounding box from all model elements
            glm::vec3 minBounds(16.0f);
            glm::vec3 maxBounds(0.0f);

            for (const auto& element : model->elements) {
                minBounds = glm::min(minBounds, element.from);
                maxBounds = glm::max(maxBounds, element.to);
            }

            // Convert from block space (0-16) to normalized space (0-1)
            minBounds /= 16.0f;
            maxBounds /= 16.0f;

            shapeCache_[state.id] = BlockShape::fromBounds(minBounds, maxBounds);
        }
    }

    return shapeCache_[state.id];
}

void FaceCullingSystem::precacheAllShapes(const std::unordered_map<uint16_t, const BlockModel*>& stateToModel) {
    shapeCache_.clear();
    shapeCache_.reserve(stateToModel.size());

    spdlog::info("Pre-caching BlockShapes for {} BlockStates...", stateToModel.size());

    size_t fullCubes = 0;
    size_t emptyShapes = 0;
    size_t partialShapes = 0;

    for (const auto& [stateId, model] : stateToModel) {
        BlockState state(stateId);

        // Compute shape using Block's getOutlineShape()
        if (state.isAir()) {
            shapeCache_[stateId] = BlockShape::empty();
            emptyShapes++;
            continue;
        }

        Block* block = BlockRegistry::getBlock(state);
        if (block) {
            shapeCache_[stateId] = block->getOutlineShape(state);
        } else {
            // Fallback: compute from model
            if (!model || model->elements.empty()) {
                shapeCache_[stateId] = BlockShape::empty();
                emptyShapes++;
                continue;
            }

            // Compute bounding box from all model elements
            glm::vec3 minBounds(16.0f);
            glm::vec3 maxBounds(0.0f);

            for (const auto& element : model->elements) {
                minBounds = glm::min(minBounds, element.from);
                maxBounds = glm::max(maxBounds, element.to);
            }

            // Convert from block space (0-16) to normalized space (0-1)
            minBounds /= 16.0f;
            maxBounds /= 16.0f;

            shapeCache_[stateId] = BlockShape::fromBounds(minBounds, maxBounds);
        }

        // Count shape types
        if (shapeCache_[stateId].isEmpty()) {
            emptyShapes++;
        } else if (shapeCache_[stateId].isFullCube()) {
            fullCubes++;
        } else {
            partialShapes++;
        }
    }

    spdlog::info("BlockShape cache built: {} full cubes, {} partial, {} empty (total: {})",
                 fullCubes, partialShapes, emptyShapes, shapeCache_.size());
}

bool FaceCullingSystem::geometricComparison(
    const std::shared_ptr<VoxelSet>& ourFace,
    const std::shared_ptr<VoxelSet>& neighborFace
) {
    // ========================================================================
    // Voxel-level matching with ONLY_FIRST predicate
    // Returns: true if ANY part of shape1 is NOT covered by shape2
    //
    // Algorithm:
    //   1. Creates merged voxel grid of both shapes
    //   2. Tests each voxel: shape1.contains(voxel) && !shape2.contains(voxel)
    //   3. Returns true if any voxel matches (exposed geometry)
    //
    // Implementation:
    //   - Use the matchesAnywhere function from VoxelSet.cpp
    //   - Directly tests voxel-by-voxel coverage
    //   - Returns true if ourFace has any voxel that neighborFace doesn't
    // ========================================================================

    // Handle null cases
    if (!ourFace || !neighborFace) {
        // If our face is null (empty), nothing to draw
        if (!ourFace) return false;
        // If neighbor face is null (empty), our face is exposed
        return true;
    }

    // Use the voxel-level matchesAnywhere comparison
    // Returns true if ourFace has any voxel NOT in neighborFace (exposed)
    return matchesAnywhere(*ourFace, *neighborFace);
}

void FaceCullingSystem::clearCache() {
    s_cache.clear();
    shapeCache_.clear();
}

} // namespace FarHorizon