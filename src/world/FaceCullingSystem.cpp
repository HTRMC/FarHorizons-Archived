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
    // Minecraft's VoxelShapes.matchesAnywhere(shape1, shape2, ONLY_FIRST)
    // Returns: true if ANY part of shape1 is NOT covered by shape2
    //
    // In other words:
    //   - true = face should be drawn (some geometry is exposed)
    //   - false = face should be culled (completely covered)
    // ========================================================================

    // With face bounds already extracted, we just check:
    // 1. Do the faces actually touch at the boundary (depth check)?
    // 2. Is our 2D face area completely covered by neighbor's 2D face area?

    constexpr float EPSILON = 0.001f;

    // Check if the faces actually touch (depth alignment)
    // For adjacent blocks, our face and neighbor's face should be at matching depths
    // E.g., our North face (depth=0) should touch neighbor's South face (depth=1)
    float depthDiff = std::abs(ourFace.depth - neighborFace.depth);
    if (depthDiff > EPSILON) {
        // Faces don't align properly - this shouldn't normally happen for adjacent blocks
        // but could occur with very small partial shapes
        return true;  // Conservative: draw the face
    }

    // Check 2D coverage: is our face rectangle completely covered by neighbor's?
    // If ANY part of our face extends beyond neighbor's face, we must draw
    bool exposedOnLeft = ourFace.min.x < neighborFace.min.x - EPSILON;
    bool exposedOnRight = ourFace.max.x > neighborFace.max.x + EPSILON;
    bool exposedOnBottom = ourFace.min.y < neighborFace.min.y - EPSILON;
    bool exposedOnTop = ourFace.max.y > neighborFace.max.y + EPSILON;

    // If any edge is exposed, must draw
    if (exposedOnLeft || exposedOnRight || exposedOnBottom || exposedOnTop) {
        return true;  // Some part of our face is not covered
    }

    // Our face is completely covered by neighbor's face - can cull
    return false;
}

void FaceCullingSystem::clearCache() {
    s_cache.clear();
    m_shapeCache.clear();
}

} // namespace FarHorizon