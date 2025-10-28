#include "FaceCullingSystem.hpp"
#include "Chunk.hpp"
#include "BlockModel.hpp"
#include "BlockRegistry.hpp"
#include <spdlog/spdlog.h>

namespace VoxelEngine {

// ============================================================================
// FaceCullCache Implementation
// ============================================================================

std::optional<bool> FaceCullCache::get(const ShapePair& key) {
    auto it = m_map.find(key);
    if (it != m_map.end()) {
        // Move to front (most recently used)
        m_list.splice(m_list.begin(), m_list, it->second);
        return it->second->second;  // Return cached result
    }
    return std::nullopt;  // Cache miss
}

void FaceCullCache::put(const ShapePair& key, bool shouldCull) {
    auto it = m_map.find(key);

    if (it != m_map.end()) {
        // Update existing entry and move to front
        it->second->second = shouldCull;
        m_list.splice(m_list.begin(), m_list, it->second);
    } else {
        // New entry
        if (m_map.size() >= MAX_SIZE) {
            // Evict least recently used (back of list)
            auto last = m_list.end();
            --last;
            m_map.erase(last->first);
            m_list.pop_back();
        }

        // Insert at front
        m_list.push_front({key, shouldCull});
        m_map[key] = m_list.begin();
    }
}

void FaceCullCache::clear() {
    m_map.clear();
    m_list.clear();
}

// ============================================================================
// FaceCullingSystem Implementation
// ============================================================================

// Thread-local cache instance (like Minecraft's ThreadLocal<Object2ByteLinkedOpenHashMap>)
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
    // Minecraft: if (voxelShape == VoxelShapes.fullCube()) return false;
    if (neighborShape.getType() == ShapeType::FULL_CUBE) {
        return false;  // Completely covered, don't draw
    }

    // ========================================================================
    // FAST PATH 2: Special block logic (glass-to-glass, etc.)
    // ========================================================================
    // Minecraft: if (state.isSideInvisible(otherState, side)) return false;
    //
    // For VulkanVoxel, we could implement special cases here:
    // - Glass blocks adjacent to other glass blocks
    // - Water adjacent to water
    // - Leaves adjacent to leaves (optional transparency culling)
    //
    // TODO: Implement BlockState.isSideInvisible() equivalent if needed
    // For now, we skip this fast path

    // ========================================================================
    // FAST PATH 3: Neighbor is empty/air → draw face
    // ========================================================================
    // Minecraft: if (voxelShape == VoxelShapes.empty()) return true;
    if (neighborShape.getType() == ShapeType::EMPTY || neighborState.isAir()) {
        return true;  // Nothing blocking, must draw
    }

    // ========================================================================
    // FAST PATH 4: Our face is empty → draw face
    // ========================================================================
    // Minecraft: if (voxelShape2 == VoxelShapes.empty()) return true;
    if (currentShape.getType() == ShapeType::EMPTY) {
        return true;  // We have no geometry on this face, shouldn't reach here
    }

    // ========================================================================
    // SLOW PATH: Geometric comparison with caching
    // ========================================================================
    // Minecraft:
    //   VoxelShapePair pair = new VoxelShapePair(ourShape, neighborShape);
    //   byte cached = FACE_CULL_MAP.get().getAndMoveToFirst(pair);
    //   if (cached != Byte.MAX_VALUE) return (cached != 0);
    //   boolean result = VoxelShapes.matchesAnywhere(ourShape, neighborShape, ONLY_FIRST);
    //   ... cache result ...
    //   return result;

    // Both blocks have partial geometry - need geometric comparison
    ShapePair pair{&currentShape, &neighborShape};

    // Check cache
    auto cached = s_cache.get(pair);
    if (cached.has_value()) {
        return cached.value();  // Return cached result
    }

    // Cache miss - perform geometric comparison
    bool shouldDraw = geometricComparison(currentShape, neighborShape, face);

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
    // Minecraft ChunkRegion.getBlockState() logic:
    //   1. Convert block position to chunk coordinates
    //   2. Check if chunk is within dependency radius
    //   3. Return block state or crash if out of range
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
    // Minecraft ChunkRegion.getChunk() boundary check:
    //   - Calculates Chebyshev distance to center chunk
    //   - Checks if distance < directDependencies.size()
    //   - Returns chunk if within range, crashes otherwise
    //
    // For VulkanVoxel during rendering:
    //   - We simply check if neighbor chunk exists
    //   - If not loaded, treat as AIR (causes faces at render edge to draw)
    //   - This matches Minecraft's behavior at render distance boundary
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
    auto it = m_shapeCache.find(state.id);
    if (it != m_shapeCache.end()) {
        return it->second;
    }

    // Cache miss - compute shape and store it
    if (state.isAir() || !model || model->elements.empty()) {
        m_shapeCache[state.id] = BlockShape(ShapeType::EMPTY);
        return m_shapeCache[state.id];
    }

    // Compute bounding box from all model elements
    glm::vec3 minBounds(16.0f);
    glm::vec3 maxBounds(0.0f);

    for (const auto& element : model->elements) {
        minBounds = glm::min(minBounds, element.from);
        maxBounds = glm::max(maxBounds, element.to);
    }

    // Check if it's a full cube
    constexpr float EPSILON = 0.01f;
    bool isFullCube =
        minBounds.x < EPSILON && minBounds.y < EPSILON && minBounds.z < EPSILON &&
        maxBounds.x > (16.0f - EPSILON) && maxBounds.y > (16.0f - EPSILON) && maxBounds.z > (16.0f - EPSILON);

    if (isFullCube) {
        m_shapeCache[state.id] = BlockShape(ShapeType::FULL_CUBE);
    } else {
        m_shapeCache[state.id] = BlockShape::partial(minBounds / 16.0f, maxBounds / 16.0f);
    }

    return m_shapeCache[state.id];
}

void FaceCullingSystem::rebuildShapeCache(const std::function<const BlockModel*(uint16_t)>& getModelFunc) {
    m_shapeCache.clear();

    // Pre-cache shapes for all block states (optional optimization)
    // This would iterate through all registered BlockStates and compute their shapes
    // For now, we use lazy caching (compute on first use)
}

bool FaceCullingSystem::geometricComparison(
    const BlockShape& ourShape,
    const BlockShape& neighborShape,
    FaceDirection face
) {
    // ========================================================================
    // Minecraft's VoxelShapes.matchesAnywhere(shape1, shape2, ONLY_FIRST)
    // Returns: true if ANY part of shape1 is NOT covered by shape2
    //
    // In other words:
    //   - true = face should be drawn (some geometry is exposed)
    //   - false = face should be culled (completely covered)
    // ========================================================================

    // For partial shapes, we do a simplified bounding box check
    // In Minecraft, this does a complex voxel-based comparison
    // For VulkanVoxel, we check if the face is completely covered

    // Get the bounding boxes in 0-1 space
    glm::vec3 ourMin = ourShape.getMin();
    glm::vec3 ourMax = ourShape.getMax();
    glm::vec3 neighborMin = neighborShape.getMin();
    glm::vec3 neighborMax = neighborShape.getMax();

    // Project onto the face plane and check if our face area is covered
    // by the neighbor's opposite face area
    switch (face) {
        case FaceDirection::DOWN:  // -Y face
        case FaceDirection::UP:    // +Y face
            // Check XZ coverage
            return (neighborMin.x > ourMin.x + 0.001f) ||
                   (neighborMax.x < ourMax.x - 0.001f) ||
                   (neighborMin.z > ourMin.z + 0.001f) ||
                   (neighborMax.z < ourMax.z - 0.001f);

        case FaceDirection::NORTH: // -Z face
        case FaceDirection::SOUTH: // +Z face
            // Check XY coverage
            return (neighborMin.x > ourMin.x + 0.001f) ||
                   (neighborMax.x < ourMax.x - 0.001f) ||
                   (neighborMin.y > ourMin.y + 0.001f) ||
                   (neighborMax.y < ourMax.y - 0.001f);

        case FaceDirection::WEST:  // -X face
        case FaceDirection::EAST:  // +X face
            // Check YZ coverage
            return (neighborMin.y > ourMin.y + 0.001f) ||
                   (neighborMax.y < ourMax.y - 0.001f) ||
                   (neighborMin.z > ourMin.z + 0.001f) ||
                   (neighborMax.z < ourMax.z - 0.001f);
    }

    // Default: draw the face (conservative)
    return true;
}

void FaceCullingSystem::clearCache() {
    s_cache.clear();
    m_shapeCache.clear();
}

} // namespace VoxelEngine