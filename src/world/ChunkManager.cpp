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
    unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency() / 2);
    for (unsigned int i = 0; i < numThreads; i++) {
        m_workerThreads.emplace_back(&ChunkManager::meshWorker, this);
    }
    spdlog::info("ChunkManager initialized with {} mesh worker threads", numThreads);
}

ChunkManager::~ChunkManager() {
    m_running = false;
    m_queueCV.notify_all();
    for (auto& thread : m_workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
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

ChunkPosition ChunkManager::worldToChunkPos(const glm::vec3& worldPos) const {
    return {
        static_cast<int32_t>(std::floor(worldPos.x / CHUNK_SIZE)),
        static_cast<int32_t>(std::floor(worldPos.y / CHUNK_SIZE)),
        static_cast<int32_t>(std::floor(worldPos.z / CHUNK_SIZE))
    };
}

void ChunkManager::update(const glm::vec3& cameraPosition) {
    ChunkPosition cameraChunkPos = worldToChunkPos(cameraPosition);

    if (cameraChunkPos != m_lastCameraChunkPos) {
        loadChunksAroundPosition(cameraChunkPos);
        unloadDistantChunks(cameraChunkPos);
        m_lastCameraChunkPos = cameraChunkPos;
    }
}

void ChunkManager::loadChunksAroundPosition(const ChunkPosition& centerPos) {
    // Collect chunks that need to be loaded
    std::vector<ChunkPosition> chunksToLoad;

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
                            chunksToLoad.push_back(pos);
                        }
                    }
                }
            }
        }
    }

    // Queue chunks for generation on worker threads (don't generate on main thread!)
    if (!chunksToLoad.empty()) {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        for (const auto& pos : chunksToLoad) {
            m_meshQueue.push(pos);
        }
        m_queueCV.notify_all();
        spdlog::trace("Queued {} chunks for loading", chunksToLoad.size());
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

Chunk* ChunkManager::loadChunk(const ChunkPosition& pos) {
    auto chunk = std::make_unique<Chunk>(pos);
    chunk->generate();

    Chunk* chunkPtr = chunk.get();
    m_chunks[pos] = std::move(chunk);

    if (!chunkPtr->isEmpty()) {
        spdlog::trace("Loaded chunk at ({}, {}, {})", pos.x, pos.y, pos.z);
        chunkPtr->markDirty();

        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_meshQueue.push(pos);
        m_queueCV.notify_one();
    }

    return chunkPtr;
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

                        // Create packed lighting (uniform bright light for now)
                        uint32_t lightIndex = static_cast<uint32_t>(mesh.lighting.size());
                        mesh.lighting.push_back(PackedLighting::uniform(31, 31, 31));  // Full bright

                        // Create FaceData
                        FaceData faceData = FaceData::pack(
                            bx, by, bz,          // Block position within chunk
                            false,                // isBackFace (not used yet)
                            lightIndex,           // Lighting buffer index
                            faceTextureIndex,     // Texture index
                            quadIndex             // QuadInfo buffer index
                        );

                        mesh.faces.push_back(faceData);
                    }
                }
            }
        }
    }

    return mesh;
}

void ChunkManager::meshWorker() {
    while (m_running) {
        ChunkPosition pos;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCV.wait(lock, [this] { return !m_meshQueue.empty() || !m_running; });

            if (!m_running) {
                break;
            }

            if (m_meshQueue.empty()) {
                continue;
            }

            pos = m_meshQueue.front();
            m_meshQueue.pop();
        }

        // Check if chunk already exists
        bool chunkExists = false;
        {
            std::lock_guard<std::mutex> lock(m_chunksMutex);
            chunkExists = (m_chunks.find(pos) != m_chunks.end());
        }

        // If chunk doesn't exist, generate it first
        bool wasNewlyCreated = false;
        Chunk* chunkPtr = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_chunksMutex);
            auto it = m_chunks.find(pos);
            if (it != m_chunks.end()) {
                chunkPtr = it->second.get();
            }
        }

        if (!chunkPtr) {
            auto chunk = std::make_unique<Chunk>(pos);
            chunk->generate();
            chunk->markDirty();  // Mark new chunks dirty so they get meshed

            std::lock_guard<std::mutex> lock(m_chunksMutex);
            // Double-check it wasn't added by another thread
            if (m_chunks.find(pos) == m_chunks.end()) {
                chunkPtr = chunk.get();
                m_chunks[pos] = std::move(chunk);
                wasNewlyCreated = true;
                spdlog::trace("Worker generated chunk at ({}, {}, {})", pos.x, pos.y, pos.z);
            } else {
                chunkPtr = m_chunks[pos].get();
            }
        }

        // Check if chunk is actually dirty before remeshing
        bool needsRemesh = false;
        {
            std::lock_guard<std::mutex> lock(m_chunksMutex);
            if (chunkPtr && chunkPtr->isDirty()) {
                needsRemesh = true;
            }
        }

        if (!needsRemesh) {
            continue;
        }

        // Wait for all required neighbors to load (Minecraft's ChunkRegion approach)
        if (!areNeighborsLoadedForMeshing(pos)) {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_meshQueue.push(pos);  // Re-queue for later
            continue;
        }

        // Clear dirty flag before meshing to allow new requests while we're meshing
        {
            std::lock_guard<std::mutex> lock(m_chunksMutex);
            if (chunkPtr) {
                chunkPtr->clearDirty();
            }
        }

        // Now generate the mesh
        CompactChunkMesh mesh;
        {
            std::lock_guard<std::mutex> lock(m_chunksMutex);
            const Chunk* chunk = getChunk(pos);
            if (chunk && !chunk->isEmpty()) {
                mesh = generateChunkMesh(chunk);
            }
        }

        if (!mesh.faces.empty()) {
            std::lock_guard<std::mutex> lock(m_readyMutex);
            m_readyMeshes.push(std::move(mesh));
        }

        // Only queue neighbors for remeshing when this chunk was newly created
        // Don't queue self - let neighbors queue us back when they're created
        // This ensures we only remesh after neighbors exist
        if (wasNewlyCreated) {
            queueNeighborRemesh(pos);
        }
    }
}

bool ChunkManager::hasReadyMeshes() const {
    std::lock_guard<std::mutex> lock(m_readyMutex);
    return !m_readyMeshes.empty();
}

std::vector<CompactChunkMesh> ChunkManager::getReadyMeshes() {
    std::vector<CompactChunkMesh> meshes;

    std::lock_guard<std::mutex> lock(m_readyMutex);
    while (!m_readyMeshes.empty()) {
        meshes.push_back(std::move(m_readyMeshes.front()));
        m_readyMeshes.pop();
    }

    return meshes;
}

void ChunkManager::queueNeighborRemesh(const ChunkPosition& pos) {
    std::vector<ChunkPosition> neighborsToQueue;

    // Queue the 6 face-adjacent neighbors for remeshing
    for (const auto& offset : ChunkPosition::getFaceNeighborOffsets()) {
        ChunkPosition neighborPos = pos.getNeighbor(offset.x, offset.y, offset.z);

        // Check if neighbor chunk exists and mark it dirty
        {
            std::lock_guard<std::mutex> chunkLock(m_chunksMutex);
            auto it = m_chunks.find(neighborPos);
            if (it != m_chunks.end()) {
                Chunk* neighborChunk = it->second.get();
                neighborChunk->markDirty();
                neighborsToQueue.push_back(neighborPos);
            }
        }
    }

    // Queue all neighbors at once
    if (!neighborsToQueue.empty()) {
        std::lock_guard<std::mutex> queueLock(m_queueMutex);
        for (const auto& neighborPos : neighborsToQueue) {
            m_meshQueue.push(neighborPos);
        }
        m_queueCV.notify_all();
    }
}

void ChunkManager::queueChunkRemesh(const ChunkPosition& pos) {
    bool shouldQueue = false;
    {
        std::lock_guard<std::mutex> chunkLock(m_chunksMutex);
        auto it = m_chunks.find(pos);
        if (it != m_chunks.end()) {
            Chunk* chunk = it->second.get();
            chunk->markDirty();
            shouldQueue = true;
        }
    }

    if (shouldQueue) {
        std::lock_guard<std::mutex> queueLock(m_queueMutex);
        m_meshQueue.push(pos);
        m_queueCV.notify_one();
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