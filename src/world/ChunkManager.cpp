#include "ChunkManager.hpp"
#include "FaceUtils.hpp"
#include "BlockRegistry.hpp"
#include <cmath>
#include <spdlog/spdlog.h>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>

namespace VoxelEngine {

// ===== Blockstate Rotation Helper Functions =====

// Apply Y-axis rotation (vertical axis) to a position (in 0-1 space)
static glm::vec3 applyYRotation(const glm::vec3& pos, int degrees) {
    if (degrees == 0) return pos;

    // Rotate around center (0.5, 0.5, 0.5)
    glm::vec3 centered = pos - glm::vec3(0.5f);
    glm::vec3 rotated;

    float rad = glm::radians(static_cast<float>(degrees));
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);

    rotated.x = centered.x * cosA - centered.z * sinA;
    rotated.y = centered.y;
    rotated.z = centered.x * sinA + centered.z * cosA;

    return rotated + glm::vec3(0.5f);
}

// Apply X-axis rotation (horizontal axis) to a position (in 0-1 space)
static glm::vec3 applyXRotation(const glm::vec3& pos, int degrees) {
    if (degrees == 0) return pos;

    // Rotate around center (0.5, 0.5, 0.5)
    glm::vec3 centered = pos - glm::vec3(0.5f);
    glm::vec3 rotated;

    float rad = glm::radians(static_cast<float>(degrees));
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);

    rotated.x = centered.x;
    rotated.y = centered.y * cosA - centered.z * sinA;
    rotated.z = centered.y * sinA + centered.z * cosA;

    return rotated + glm::vec3(0.5f);
}

// Rotate a face direction based on Y rotation
static FaceDirection rotateYFace(FaceDirection face, int degrees) {
    if (degrees == 0) return face;

    // Y rotation only affects horizontal faces
    if (face == FaceDirection::UP || face == FaceDirection::DOWN) {
        return face;
    }

    // Map face to index: NORTH=0, EAST=1, SOUTH=2, WEST=3
    int faceIdx = 0;
    if (face == FaceDirection::NORTH) faceIdx = 0;
    else if (face == FaceDirection::EAST) faceIdx = 1;
    else if (face == FaceDirection::SOUTH) faceIdx = 2;
    else if (face == FaceDirection::WEST) faceIdx = 3;

    // Rotate by 90-degree increments
    int steps = degrees / 90;
    faceIdx = (faceIdx + steps) % 4;

    // Map back to face direction
    if (faceIdx == 0) return FaceDirection::NORTH;
    if (faceIdx == 1) return FaceDirection::EAST;
    if (faceIdx == 2) return FaceDirection::SOUTH;
    return FaceDirection::WEST;
}

// Rotate a face direction based on X rotation
static FaceDirection rotateXFace(FaceDirection face, int degrees) {
    if (degrees == 0) return face;

    // X rotation affects vertical and Z-axis faces
    if (degrees == 90) {
        if (face == FaceDirection::UP) return FaceDirection::NORTH;
        if (face == FaceDirection::NORTH) return FaceDirection::DOWN;
        if (face == FaceDirection::DOWN) return FaceDirection::SOUTH;
        if (face == FaceDirection::SOUTH) return FaceDirection::UP;
    } else if (degrees == 180) {
        if (face == FaceDirection::UP) return FaceDirection::DOWN;
        if (face == FaceDirection::DOWN) return FaceDirection::UP;
        if (face == FaceDirection::NORTH) return FaceDirection::SOUTH;
        if (face == FaceDirection::SOUTH) return FaceDirection::NORTH;
    } else if (degrees == 270) {
        if (face == FaceDirection::UP) return FaceDirection::SOUTH;
        if (face == FaceDirection::SOUTH) return FaceDirection::DOWN;
        if (face == FaceDirection::DOWN) return FaceDirection::NORTH;
        if (face == FaceDirection::NORTH) return FaceDirection::UP;
    }

    return face;
}

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

void ChunkManager::clearAllChunks() {
    size_t count = 0;
    {
        std::lock_guard<std::mutex> lock(m_chunksMutex);
        count = m_chunks.size();
        m_chunks.clear();
    }

    // Clear mesh queues
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (!m_meshQueue.empty()) {
            m_meshQueue.pop();
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_readyMutex);
        while (!m_readyMeshes.empty()) {
            m_readyMeshes.pop();
        }
    }

    // Reset last camera position to force reload on next update
    m_lastCameraChunkPos = {INT32_MAX, INT32_MAX, INT32_MAX};

    spdlog::info("Cleared all chunks (unloaded {} chunks)", count);
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
    std::lock_guard<std::mutex> lock(m_chunksMutex);
    auto it = m_chunks.find(pos);
    if (it != m_chunks.end()) {
        return it->second.get();
    }
    return nullptr;
}

const Chunk* ChunkManager::getChunk(const ChunkPosition& pos) const {
    std::lock_guard<std::mutex> lock(m_chunksMutex);
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

                // Get the block variant (model + rotation) from cache (fast O(1) lookup!)
                const BlockStateVariant* variant = m_modelManager.getVariantByStateId(state.id);
                const BlockModel* model = variant ? variant->model : m_modelManager.getModelByStateId(state.id);

                if (!model || model->elements.empty()) {
                    continue;  // Skip if no model
                }

                // Get rotation values (0 if no variant data)
                int rotationX = variant ? variant->rotationX : 0;
                int rotationY = variant ? variant->rotationY : 0;

                // Process each element in the model
                for (const auto& element : model->elements) {
                    // Convert from 0-16 space to 0-1 space
                    glm::vec3 elemFrom = element.from / 16.0f;
                    glm::vec3 elemTo = element.to / 16.0f;

                    // Apply rotations to bounding box (Y first, then X)
                    if (rotationY != 0) {
                        elemFrom = applyYRotation(elemFrom, rotationY);
                        elemTo = applyYRotation(elemTo, rotationY);
                    }
                    if (rotationX != 0) {
                        elemFrom = applyXRotation(elemFrom, rotationX);
                        elemTo = applyXRotation(elemTo, rotationX);
                    }

                    // Ensure from < to after rotation (rotation may swap min/max)
                    glm::vec3 finalFrom = glm::min(elemFrom, elemTo);
                    glm::vec3 finalTo = glm::max(elemFrom, elemTo);

                    // Process each face of the element
                    for (const auto& [faceDir, face] : element.faces) {
                        // Apply rotations to face direction (Y first, then X)
                        FaceDirection rotatedFaceDir = faceDir;
                        if (rotationY != 0) {
                            rotatedFaceDir = rotateYFace(rotatedFaceDir, rotationY);
                        }
                        if (rotationX != 0) {
                            rotatedFaceDir = rotateXFace(rotatedFaceDir, rotationX);
                        }
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
                                // Note: m_chunksMutex is already held by caller (meshWorker)
                                BlockState neighborState = m_cullingSystem.getNeighborBlockState(
                                    chunk, chunkPos, nx, ny, nz,
                                    [this](const ChunkPosition& pos) -> const Chunk* {
                                        auto it = m_chunks.find(pos);
                                        return (it != m_chunks.end()) ? it->second.get() : nullptr;
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

                        // Generate quad geometry (use rotated face direction and rotated bounding box)
                        glm::vec3 corners[4];
                        FaceUtils::getFaceVertices(rotatedFaceDir, finalFrom, finalTo, corners);
                        int faceIndex = FaceUtils::toIndex(rotatedFaceDir);

                        // Convert UVs from Blockbench format to Vulkan format
                        glm::vec2 uvs[4];
                        FaceUtils::convertUVs(face.uv, uvs);

                        // Use cached texture index (no string operations!)
                        uint32_t faceTextureIndex = face.textureIndex;

                        // Get face normal (use rotated face direction)
                        glm::vec3 normal = FaceUtils::getFaceNormal(rotatedFaceDir);

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
        mesh.position = pos;  // Always set position so BufferManager knows which chunk this is
        {
            std::lock_guard<std::mutex> lock(m_chunksMutex);
            auto it = m_chunks.find(pos);
            if (it != m_chunks.end() && !it->second->isEmpty()) {
                mesh = generateChunkMesh(it->second.get());
            }
        }

        // Always push mesh to ready queue, even if empty
        // This allows BufferManager to remove chunks that became empty after breaking blocks
        {
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