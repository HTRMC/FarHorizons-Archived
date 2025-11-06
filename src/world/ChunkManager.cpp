#include "ChunkManager.hpp"
#include "FaceUtils.hpp"
#include "BlockRegistry.hpp"
#include <cmath>
#include <spdlog/spdlog.h>
#include <functional>

namespace VoxelEngine {

// ===== QuadInfoLibrary Implementation =====

bool QuadInfoLibrary::QuadKey::operator==(const QuadKey& other) const {
    if (textureSlot != other.textureSlot) return false;
    if (normal != other.normal) return false;
    for (int i = 0; i < 4; i++) {
        if (corners[i] != other.corners[i]) return false;
        if (uvs[i] != other.uvs[i]) return false;
    }
    return true;
}

size_t QuadInfoLibrary::QuadKeyHash::operator()(const QuadKey& key) const {
    size_t hash = std::hash<uint32_t>{}(key.textureSlot);

    // Hash normal
    hash ^= std::hash<float>{}(key.normal.x) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<float>{}(key.normal.y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<float>{}(key.normal.z) + 0x9e3779b9 + (hash << 6) + (hash >> 2);

    // Hash corners
    for (int i = 0; i < 4; i++) {
        hash ^= std::hash<float>{}(key.corners[i].x) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<float>{}(key.corners[i].y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<float>{}(key.corners[i].z) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<float>{}(key.uvs[i].x) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<float>{}(key.uvs[i].y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }

    return hash;
}

uint32_t QuadInfoLibrary::getOrCreateQuad(const glm::vec3& normal,
                                          const glm::vec3 corners[4],
                                          const glm::vec2 uvs[4],
                                          uint32_t textureSlot) {
    QuadKey key;
    key.normal = normal;
    for (int i = 0; i < 4; i++) {
        key.corners[i] = corners[i];
        key.uvs[i] = uvs[i];
    }
    key.textureSlot = textureSlot;

    auto it = m_quadMap.find(key);
    if (it != m_quadMap.end()) {
        return it->second;  // Return existing index
    }

    // Create new QuadInfo
    uint32_t index = static_cast<uint32_t>(m_quads.size());
    QuadInfo quad;
    quad.normal = normal;
    quad._padding0 = 0.0f;
    quad.corner0 = corners[0];
    quad._padding1 = 0.0f;
    quad.corner1 = corners[1];
    quad._padding2 = 0.0f;
    quad.corner2 = corners[2];
    quad._padding3 = 0.0f;
    quad.corner3 = corners[3];
    quad._padding4 = 0.0f;
    quad.uv0 = uvs[0];
    quad.uv1 = uvs[1];
    quad.uv2 = uvs[2];
    quad.uv3 = uvs[3];
    quad.textureSlot = textureSlot;
    quad._padding5 = 0;

    m_quads.push_back(quad);
    m_quadMap[key] = index;

    return index;
}

// ===== ChunkManager Implementation =====

ChunkManager::ChunkManager() {
    // Create separate thread pools for generation and meshing
    // Generation: Use 75% of cores (terrain generation is CPU-intensive)
    // Meshing: Use 25% of cores (meshing is lighter, mostly geometry processing)
    unsigned int totalCores = std::thread::hardware_concurrency();
    unsigned int genThreads = std::max(1u, (totalCores * 3) / 4);
    unsigned int meshThreads = std::max(1u, totalCores / 4);

    m_generationPool = std::make_unique<ThreadPool>(genThreads, "ChunkGeneration");
    m_meshingPool = std::make_unique<ThreadPool>(meshThreads, "ChunkMeshing");

    spdlog::info("ChunkManager initialized with {} generation threads and {} meshing threads",
                 genThreads, meshThreads);
}

ChunkManager::~ChunkManager() {
    // Thread pools will automatically shut down and join their workers
}

void ChunkManager::setWorldGenerator(std::shared_ptr<WorldGenerator> generator) {
    m_worldGenerator = std::move(generator);
}

void ChunkManager::initializeBlockModels() {
    m_modelManager.initialize();
}

void ChunkManager::preloadBlockStateModels() {
    m_modelManager.preloadBlockStateModels();
}

void ChunkManager::registerTexture(const std::string& textureName, uint32_t textureIndex) {
    m_modelManager.registerTexture(textureName, textureIndex);
}

std::vector<std::string> ChunkManager::getRequiredTextures() const {
    return m_modelManager.getAllTextureNames();
}

void ChunkManager::cacheTextureIndices() {
    m_modelManager.cacheTextureIndices();
}

void ChunkManager::precacheBlockShapes() {
    // Pre-compute BlockShapes for all BlockStates (eliminates lazy loading stutter)
    m_cullingSystem.precacheAllShapes(m_modelManager.getStateToModelMap());
}

void ChunkManager::setRenderDistance(int32_t distance) {
    if (m_renderDistance != distance) {
        m_renderDistance = distance;
        m_renderDistanceChanged = true;
    }
}

ChunkPosition ChunkManager::worldToChunkPos(const glm::vec3& worldPos) const {
    return {
        static_cast<int32_t>(std::floor(worldPos.x / CHUNK_SIZE)),
        static_cast<int32_t>(std::floor(worldPos.y / CHUNK_SIZE)),
        static_cast<int32_t>(std::floor(worldPos.z / CHUNK_SIZE))
    };
}

void ChunkManager::update(const glm::vec3& cameraPosition) {
    ChunkPosition cameraChunkPos = worldToChunkPos(cameraPosition);

    if (cameraChunkPos != m_lastCameraChunkPos || m_renderDistanceChanged) {
        loadChunksAroundPosition(cameraChunkPos);
        unloadDistantChunks(cameraChunkPos);
        m_lastCameraChunkPos = cameraChunkPos;
        m_renderDistanceChanged = false;
    }
}

void ChunkManager::loadChunksAroundPosition(const ChunkPosition& centerPos) {
    // Collect chunks that need to be generated
    std::vector<ChunkPosition> chunksToGenerate;

    {
        std::lock_guard<std::mutex> lock(m_chunksMutex);
        for (int32_t x = -m_renderDistance; x <= m_renderDistance; x++) {
            for (int32_t y = -m_renderDistance; y <= m_renderDistance; y++) {
                for (int32_t z = -m_renderDistance; z <= m_renderDistance; z++) {
                    ChunkPosition pos = {
                        centerPos.x + x,
                        centerPos.y + y,
                        centerPos.z + z
                    };

                    float distance = std::sqrt(x*x + y*y + z*z);
                    if (distance <= m_renderDistance) {
                        if (m_chunks.find(pos) == m_chunks.end()) {
                            chunksToGenerate.push_back(pos);
                        }
                    }
                }
            }
        }
    }

    // Queue chunks for generation on dedicated generation thread pool (lock-free!)
    if (!chunksToGenerate.empty()) {
        for (const auto& pos : chunksToGenerate) {
            generateChunkAsync(pos);
        }
        spdlog::trace("Queued {} chunks for generation", chunksToGenerate.size());
    }
}

void ChunkManager::unloadDistantChunks(const ChunkPosition& centerPos) {
    std::vector<ChunkPosition> chunksToUnload;

    {
        std::lock_guard<std::mutex> lock(m_chunksMutex);
        for (const auto& [pos, chunk] : m_chunks) {
            if (pos.distanceTo(centerPos) > m_renderDistance + 1) {
                chunksToUnload.push_back(pos);
            }
        }

        if (!chunksToUnload.empty()) {
            for (const auto& pos : chunksToUnload) {
                m_chunks.erase(pos);
            }
        }
    }

    if (!chunksToUnload.empty()) {
        spdlog::debug("Unloaded {} chunks", chunksToUnload.size());
    }
}

void ChunkManager::clearAllChunks() {
    // Wait for pending generation and meshing tasks
    m_generationPool->waitForCompletion();
    m_meshingPool->waitForCompletion();

    size_t count = 0;
    {
        std::lock_guard<std::mutex> lock(m_chunksMutex);
        count = m_chunks.size();
        m_chunks.clear();
    }

    // Clear ready meshes queue (lock-free)
    CompactChunkMesh dummy;
    while (m_readyMeshes.try_dequeue(dummy)) {
        // Drain the queue
    }

    // Reset last camera position to force reload on next update
    m_lastCameraChunkPos = {INT32_MAX, INT32_MAX, INT32_MAX};

    spdlog::info("Cleared all chunks (unloaded {} chunks)", count);
}

// Async chunk generation (runs on generation thread pool)
void ChunkManager::generateChunkAsync(const ChunkPosition& pos) {
    m_generationPool->enqueue([this, pos]() {
        // Generate chunk terrain
        auto chunk = std::make_unique<Chunk>(pos);
        chunk->generate(m_worldGenerator.get());
        chunk->markDirty();

        bool wasAdded = false;
        {
            std::lock_guard<std::mutex> lock(m_chunksMutex);
            // Only add if not already present (race condition check)
            if (m_chunks.find(pos) == m_chunks.end()) {
                m_chunks[pos] = std::move(chunk);
                wasAdded = true;
                spdlog::trace("Generated chunk at ({}, {}, {})", pos.x, pos.y, pos.z);
            }
        }

        // Queue for meshing if successfully added and neighbors exist
        if (wasAdded) {
            meshChunkAsync(pos);
            queueNeighborRemesh(pos);  // Remesh neighbors too
        }
    });
}

// Async chunk meshing (runs on meshing thread pool)
void ChunkManager::meshChunkAsync(const ChunkPosition& pos) {
    m_meshingPool->enqueue([this, pos]() {
        // Check if all neighbors are loaded (required for face culling)
        if (!areNeighborsLoadedForMeshing(pos)) {
            // Re-queue for later when neighbors are ready
            std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Back off briefly
            meshChunkAsync(pos);  // Retry
            return;
        }

        // Check if chunk still exists and needs meshing
        bool needsMesh = false;
        {
            std::lock_guard<std::mutex> lock(m_chunksMutex);
            auto it = m_chunks.find(pos);
            if (it != m_chunks.end() && it->second->isDirty()) {
                it->second->clearDirty();
                needsMesh = true;
            }
        }

        if (!needsMesh) {
            return;  // Already meshed or chunk was unloaded
        }

        // Generate the mesh
        CompactChunkMesh mesh;
        mesh.position = pos;
        {
            std::lock_guard<std::mutex> lock(m_chunksMutex);
            const Chunk* chunk = getChunk(pos);
            if (chunk && !chunk->isEmpty()) {
                mesh = generateChunkMesh(chunk);
                spdlog::trace("Meshed chunk at ({}, {}, {})", pos.x, pos.y, pos.z);
            }
        }

        // Push to ready queue (lock-free!)
        m_readyMeshes.enqueue(std::move(mesh));
    });
}

Chunk* ChunkManager::getChunk(const ChunkPosition& pos) {
    auto it = m_chunks.find(pos);
    if (it != m_chunks.end()) {
        return it->second.get();
    }
    return nullptr;
}

const Chunk* ChunkManager::getChunk(const ChunkPosition& pos) const {
    auto it = m_chunks.find(pos);
    if (it != m_chunks.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool ChunkManager::hasChunk(const ChunkPosition& pos) const {
    std::lock_guard<std::mutex> lock(m_chunksMutex);
    return m_chunks.find(pos) != m_chunks.end();
}

CompactChunkMesh ChunkManager::generateChunkMesh(const Chunk* chunk) const {
    CompactChunkMesh mesh;
    mesh.position = chunk->getPosition();

    if (chunk->isEmpty()) {
        return mesh;
    }

    const ChunkPosition& chunkPos = chunk->getPosition();

    // Iterate through all blocks in the chunk
    for (uint32_t bx = 0; bx < CHUNK_SIZE; bx++) {
        for (uint32_t by = 0; by < CHUNK_SIZE; by++) {
            for (uint32_t bz = 0; bz < CHUNK_SIZE; bz++) {
                // Get blockstate
                BlockState state = chunk->getBlockState(bx, by, bz);
                if (state.isAir()) {
                    continue;
                }

                // Get the block model from cache (fast O(1) lookup!)
                const BlockModel* model = m_modelManager.getModelByStateId(state.id);
                if (!model || model->elements.empty()) {
                    continue;  // Skip if no model
                }

                // Process each element in the model
                for (const auto& element : model->elements) {
                    // Convert from 0-16 space to 0-1 space
                    glm::vec3 elemFrom = element.from / 16.0f;
                    glm::vec3 elemTo = element.to / 16.0f;

                    // Process each face of the element
                    for (const auto& [faceDir, face] : element.faces) {
                        // Check cullface - should we skip this face?
                        bool shouldRender = true;

                        if (face.cullface.has_value()) {
                            // Only cull if this element actually reaches the block boundary
                            if (FaceUtils::faceReachesBoundary(face.cullface.value(), elemFrom, elemTo)) {
                                // ================================================================
                                // MINECRAFT-STYLE FACE CULLING SYSTEM
                                // ================================================================

                                // Get neighbor position
                                int faceIndex = FaceUtils::toIndex(face.cullface.value());
                                int nx = bx + FaceUtils::FACE_DIRS[faceIndex][0];
                                int ny = by + FaceUtils::FACE_DIRS[faceIndex][1];
                                int nz = bz + FaceUtils::FACE_DIRS[faceIndex][2];

                                // Safe neighbor access with chunk boundary checking
                                BlockState neighborState = m_cullingSystem.getNeighborBlockState(
                                    chunk, chunkPos, nx, ny, nz,
                                    [this](const ChunkPosition& pos) -> const Chunk* {
                                        return this->getChunk(pos);
                                    }
                                );

                                // Get block shapes for culling logic
                                const BlockShape& currentShape = m_cullingSystem.getBlockShape(state, model);
                                const BlockModel* neighborModel = m_modelManager.getModelByStateId(neighborState.id);
                                const BlockShape& neighborShape = m_cullingSystem.getBlockShape(neighborState, neighborModel);

                                // Use Minecraft's shouldDrawSide() logic with fast paths
                                bool shouldDrawThisFace = m_cullingSystem.shouldDrawFace(
                                    state,
                                    neighborState,
                                    face.cullface.value(),
                                    currentShape,
                                    neighborShape
                                );

                                if (!shouldDrawThisFace) {
                                    shouldRender = false;  // Face is culled
                                }
                            }
                        }

                        if (!shouldRender) {
                            continue;
                        }

                        // Generate quad geometry
                        glm::vec3 corners[4];
                        FaceUtils::getFaceVertices(faceDir, elemFrom, elemTo, corners);
                        int faceIndex = FaceUtils::toIndex(faceDir);

                        // Convert UVs from Blockbench format to Vulkan format
                        glm::vec2 uvs[4];
                        FaceUtils::convertUVs(face.uv, uvs);

                        // Use cached texture index (no string operations!)
                        uint32_t faceTextureIndex = face.textureIndex;

                        // Get face normal
                        glm::vec3 normal = FaceUtils::getFaceNormal(faceDir);

                        // Get or create QuadInfo for this geometry
                        uint32_t quadIndex = m_quadLibrary.getOrCreateQuad(normal, corners, uvs, faceTextureIndex);

                        // Determine lighting value
                        PackedLighting lighting;
                        if (face.tintindex.has_value()) {
                            // Hardcoded grass color #79C05A (RGB: 121, 192, 90)
                            // Convert to 5-bit values (0-31)
                            uint8_t grassR = static_cast<uint8_t>((121 * 31) / 255);  // ~15
                            uint8_t grassG = static_cast<uint8_t>((192 * 31) / 255);  // ~23
                            uint8_t grassB = static_cast<uint8_t>((90 * 31) / 255);   // ~11
                            lighting = PackedLighting::uniform(grassR, grassG, grassB);
                        } else {
                            // Full white lighting (no tint)
                            lighting = PackedLighting::uniform(31, 31, 31);
                        }

                        // Deduplicate lighting: find or create index for this lighting value
                        uint32_t lightIndex = 0;
                        bool found = false;
                        for (size_t i = 0; i < mesh.lighting.size(); i++) {
                            const PackedLighting& existing = mesh.lighting[i];
                            if (existing.corners[0] == lighting.corners[0] &&
                                existing.corners[1] == lighting.corners[1] &&
                                existing.corners[2] == lighting.corners[2] &&
                                existing.corners[3] == lighting.corners[3]) {
                                lightIndex = static_cast<uint32_t>(i);
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            lightIndex = static_cast<uint32_t>(mesh.lighting.size());
                            mesh.lighting.push_back(lighting);
                        }

                        // Create FaceData with deduplicated light index
                        FaceData faceData = FaceData::pack(
                            bx, by, bz,           // Block position within chunk
                            false,                 // isBackFace (not used yet)
                            lightIndex,            // Deduplicated index (0,1,2... only for unique values)
                            quadIndex              // QuadInfo buffer index (includes texture)
                        );

                        mesh.faces.push_back(faceData);
                    }
                }
            }
        }
    }

    return mesh;
}

bool ChunkManager::hasReadyMeshes() const {
    return m_readyMeshes.size_approx() > 0;
}

std::vector<CompactChunkMesh> ChunkManager::getReadyMeshes() {
    std::vector<CompactChunkMesh> meshes;

    // Dequeue all available meshes (lock-free!)
    CompactChunkMesh mesh;
    while (m_readyMeshes.try_dequeue(mesh)) {
        meshes.push_back(std::move(mesh));
    }

    return meshes;
}

void ChunkManager::queueNeighborRemesh(const ChunkPosition& pos) {
    // Queue the 6 face-adjacent neighbors for remeshing
    for (const auto& offset : ChunkPosition::getFaceNeighborOffsets()) {
        ChunkPosition neighborPos = pos.getNeighbor(offset.x, offset.y, offset.z);

        // Check if neighbor chunk exists and mark it dirty
        bool exists = false;
        {
            std::lock_guard<std::mutex> chunkLock(m_chunksMutex);
            auto it = m_chunks.find(neighborPos);
            if (it != m_chunks.end()) {
                it->second->markDirty();
                exists = true;
            }
        }

        // Queue for remeshing if it exists
        if (exists) {
            meshChunkAsync(neighborPos);
        }
    }
}

void ChunkManager::queueChunkRemesh(const ChunkPosition& pos) {
    // Mark chunk as dirty and queue for remeshing
    bool exists = false;
    {
        std::lock_guard<std::mutex> chunkLock(m_chunksMutex);
        auto it = m_chunks.find(pos);
        if (it != m_chunks.end()) {
            it->second->markDirty();
            exists = true;
        }
    }

    if (exists) {
        meshChunkAsync(pos);
    }
}

bool ChunkManager::areNeighborsLoadedForMeshing(const ChunkPosition& pos) const {
    std::lock_guard<std::mutex> lock(m_chunksMutex);

    ChunkPosition cameraChunkPos = m_lastCameraChunkPos;

    // Check all 6 face-adjacent neighbors (Minecraft's directDependencies)
    for (const auto& offset : ChunkPosition::getFaceNeighborOffsets()) {
        ChunkPosition neighborPos = pos.getNeighbor(offset.x, offset.y, offset.z);

        // Only require neighbor if it's within render distance
        if (neighborPos.distanceTo(cameraChunkPos) <= m_renderDistance) {
            if (m_chunks.find(neighborPos) == m_chunks.end()) {
                return false;  // Required neighbor missing
            }
        }
    }

    return true;  // All required neighbors loaded
}

} // namespace VoxelEngine