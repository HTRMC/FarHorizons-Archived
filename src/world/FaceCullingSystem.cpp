#include "FaceCullingSystem.hpp"
#include "Chunk.hpp"
#include "BlockModel.hpp"
#include "BlockRegistry.hpp"
#include <spdlog/spdlog.h>

namespace FarHorizon {

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
    //   VoxelShape voxelShape = otherState.getCullingFace(side.getOpposite());
    //   VoxelShape voxelShape2 = state.getCullingFace(side);
    //   VoxelShapePair pair = new VoxelShapePair(voxelShape2, voxelShape);
    //   byte cached = FACE_CULL_MAP.get().getAndMoveToFirst(pair);
    //   if (cached != Byte.MAX_VALUE) return (cached != 0);
    //   boolean result = VoxelShapes.matchesAnywhere(ourShape, neighborShape, ONLY_FIRST);
    //   ... cache result ...
    //   return result;

    // Extract face-specific geometry BEFORE caching (like Minecraft's getCullingFace)
    FaceBounds ourFace = currentShape.getCullingFace(face);
    FaceBounds neighborFace = neighborShape.getCullingFace(face);

    // Create cache key with face-extracted geometry (no direction needed!)
    ShapePair pair{ourFace, neighborFace};

    // Check cache
    auto cached = s_cache.get(pair);
    if (cached.has_value()) {
        return cached.value();  // Return cached result
    }

    // Cache miss - perform geometric comparison
    bool shouldDraw = geometricComparison(ourFace, neighborFace, face);

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

void FaceCullingSystem::precacheAllShapes(const std::unordered_map<uint16_t, const BlockModel*>& stateToModel) {
    m_shapeCache.clear();
    m_shapeCache.reserve(stateToModel.size());

    spdlog::info("Pre-caching BlockShapes for {} BlockStates...", stateToModel.size());

    size_t fullCubes = 0;
    size_t emptyShapes = 0;
    size_t partialShapes = 0;

    for (const auto& [stateId, model] : stateToModel) {
        BlockState state(stateId);

        // Compute shape
        if (state.isAir() || !model || model->elements.empty()) {
            m_shapeCache[stateId] = BlockShape(ShapeType::EMPTY);
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

        // Check if it's a full cube
        constexpr float EPSILON = 0.01f;
        bool isFullCube =
            minBounds.x < EPSILON && minBounds.y < EPSILON && minBounds.z < EPSILON &&
            maxBounds.x > (16.0f - EPSILON) && maxBounds.y > (16.0f - EPSILON) && maxBounds.z > (16.0f - EPSILON);

        if (isFullCube) {
            m_shapeCache[stateId] = BlockShape(ShapeType::FULL_CUBE);
            fullCubes++;
        } else {
            m_shapeCache[stateId] = BlockShape::partial(minBounds / 16.0f, maxBounds / 16.0f);
            partialShapes++;
        }
    }

    spdlog::info("BlockShape cache built: {} full cubes, {} partial, {} empty (total: {})",
                 fullCubes, partialShapes, emptyShapes, m_shapeCache.size());
}

bool FaceCullingSystem::geometricComparison(
    const FaceBounds& ourFace,
    const FaceBounds& neighborFace,
    FaceDirection face
) {
    // ========================================================================
    // Adapted from Minecraft's VoxelShapes.matchesAnywhere(shape1, shape2, ONLY_FIRST)
    // Returns: true if ANY part of shape1 is NOT covered by shape2
    //
    // Minecraft's approach:
    //   1. Creates voxel grid of intersection region
    //   2. Tests each voxel: shape1.contains(voxel) && !shape2.contains(voxel)
    //   3. Returns true if any voxel matches (exposed geometry)
    //
    // Our simplified approach (appropriate for bounding box shapes):
    //   1. Check if 2D face bounds overlap (shapes must touch)
    //   2. Check if our face extends beyond neighbor's face (exposed area)
    //   3. Return true if any part is exposed
    // ========================================================================

    constexpr float EPSILON = 0.001f;

    // STEP 1: Check if the faces actually overlap in 2D space
    // If they don't overlap at all, our face is definitely exposed
    bool overlapX = !(ourFace.max.x < neighborFace.min.x - EPSILON ||
                      ourFace.min.x > neighborFace.max.x + EPSILON);
    bool overlapY = !(ourFace.max.y < neighborFace.min.y - EPSILON ||
                      ourFace.min.y > neighborFace.max.y + EPSILON);

    if (!overlapX || !overlapY) {
        // No overlap - our face is completely exposed
        return true;
    }

    // STEP 2: Check if our face is completely contained within neighbor's face
    // This is the key test: is every part of our face covered by neighbor?
    //
    // For proper culling, neighbor's face must completely cover ours
    // If ANY edge of our face extends beyond neighbor's face, we're exposed
    bool exposedOnLeft = ourFace.min.x < neighborFace.min.x - EPSILON;
    bool exposedOnRight = ourFace.max.x > neighborFace.max.x + EPSILON;
    bool exposedOnBottom = ourFace.min.y < neighborFace.min.y - EPSILON;
    bool exposedOnTop = ourFace.max.y > neighborFace.max.y + EPSILON;

    if (exposedOnLeft || exposedOnRight || exposedOnBottom || exposedOnTop) {
        return true;  // Some part of our face extends beyond neighbor's coverage
    }

    // STEP 3: Our face is completely covered by neighbor's face
    // This handles the common cases correctly:
    // - Full cube vs full cube: both faces are (0,0)-(1,1), fully covers, cull
    // - Slab (height 0.5) vs full cube: slab face (0,0)-(1,1) on XZ, but only to Y=0.5
    //   Full cube face is (0,0)-(1,1) on XZ to Y=1.0, fully covers slab, cull
    // - Slab vs slab (same height): both (0,0)-(1,1) on XZ to Y=0.5, fully covers, cull
    // - Partial shape: only covered if neighbor's face is at least as large
    return false;  // Cull the face
}

void FaceCullingSystem::clearCache() {
    s_cache.clear();
    m_shapeCache.clear();
}

} // namespace FarHorizon