#include "ChunkManager.hpp"
#include "FaceUtils.hpp"
#include "BlockRegistry.hpp"
#include <tracy/Tracy.hpp>
#include <cmath>
#include <spdlog/spdlog.h>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>

namespace FarHorizon {

// ===== Blockstate Rotation Helper Functions =====

static glm::vec3 applyYRotation(const glm::vec3& pos, int degrees) {
    if (degrees == 0) return pos;

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

static glm::vec3 applyXRotation(const glm::vec3& pos, int degrees) {
    if (degrees == 0) return pos;

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

static FaceDirection rotateYFace(FaceDirection face, int degrees) {
    if (degrees == 0) return face;

    if (face == FaceDirection::UP || face == FaceDirection::DOWN) {
        return face;
    }

    int faceIdx = 0;
    if (face == FaceDirection::NORTH) faceIdx = 0;
    else if (face == FaceDirection::EAST) faceIdx = 1;
    else if (face == FaceDirection::SOUTH) faceIdx = 2;
    else if (face == FaceDirection::WEST) faceIdx = 3;

    int steps = degrees / 90;
    faceIdx = (faceIdx + steps) % 4;

    if (faceIdx == 0) return FaceDirection::NORTH;
    if (faceIdx == 1) return FaceDirection::EAST;
    if (faceIdx == 2) return FaceDirection::SOUTH;
    return FaceDirection::WEST;
}

static FaceDirection rotateXFace(FaceDirection face, int degrees) {
    if (degrees == 0) return face;

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

    hash ^= std::hash<float>{}(key.normal.x) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<float>{}(key.normal.y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<float>{}(key.normal.z) + 0x9e3779b9 + (hash << 6) + (hash >> 2);

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

    auto it = quadMap_.find(key);
    if (it != quadMap_.end()) {
        return it->second;
    }

    uint32_t index = static_cast<uint32_t>(quads_.size());
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

    quads_.push_back(quad);
    quadMap_[key] = index;

    return index;
}

// ===== ChunkManager Implementation =====

ChunkManager::ChunkManager() {
    unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency() / 2);
    for (unsigned int i = 0; i < numThreads; i++) {
        workerThreads_.emplace_back(&ChunkManager::meshWorker, this, i);
    }
    spdlog::info("ChunkManager initialized with {} mesh worker threads (lock-free architecture)", numThreads);
}

ChunkManager::~ChunkManager() {
    running_ = false;
    workQueueCV_.notify_all();
    for (auto& thread : workerThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void ChunkManager::initializeBlockModels() {
    modelManager_.initialize();
}

void ChunkManager::preloadBlockStateModels() {
    modelManager_.preloadBlockStateModels();
}

void ChunkManager::registerTexture(const std::string& textureName, uint32_t textureIndex) {
    modelManager_.registerTexture(textureName, textureIndex);
}

std::vector<std::string> ChunkManager::getRequiredTextures() const {
    return modelManager_.getAllTextureNames();
}

void ChunkManager::cacheTextureIndices() {
    modelManager_.cacheTextureIndices();
}

void ChunkManager::precacheBlockShapes() {
    cullingSystem_.precacheAllShapes(modelManager_.getStateToModelMap());
}

void ChunkManager::setRenderDistance(int32_t distance) {
    if (renderDistance_ != distance) {
        renderDistance_ = distance;
        renderDistanceChanged_ = true;
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
    ZoneScoped;
    ChunkPosition cameraChunkPos = worldToChunkPos(cameraPosition);

    ChunkPosition lastPos;
    {
        std::lock_guard<std::mutex> lock(cameraPosMutex_);
        lastPos = lastCameraChunkPos_;
    }

    if (cameraChunkPos != lastPos || renderDistanceChanged_.load()) {
        loadChunksAroundPosition(cameraChunkPos);
        unloadDistantChunks(cameraChunkPos);
        {
            std::lock_guard<std::mutex> lock(cameraPosMutex_);
            lastCameraChunkPos_ = cameraChunkPos;
        }
        renderDistanceChanged_ = false;
    }
}

void ChunkManager::loadChunksAroundPosition(const ChunkPosition& centerPos) {
    ZoneScoped;

    std::vector<MeshWorkItem> chunksToGenerate;

    for (int32_t x = -renderDistance_; x <= renderDistance_; x++) {
        for (int32_t y = -renderDistance_; y <= renderDistance_; y++) {
            for (int32_t z = -renderDistance_; z <= renderDistance_; z++) {
                ChunkPosition pos = {
                    centerPos.x + x,
                    centerPos.y + y,
                    centerPos.z + z
                };

                float distance = std::sqrt(static_cast<float>(x*x + y*y + z*z));
                if (distance <= renderDistance_) {
                    if (!storage_.contains(pos)) {
                        chunksToGenerate.push_back({pos, true});
                    }
                }
            }
        }
    }

    // Queue all chunks for generation
    if (!chunksToGenerate.empty()) {
        std::lock_guard<std::mutex> lock(workQueueMutex_);
        for (const auto& item : chunksToGenerate) {
            workQueue_.push(item);
        }
        workQueueCV_.notify_all();
        spdlog::trace("Queued {} chunks for generation", chunksToGenerate.size());
    }
}

void ChunkManager::unloadDistantChunks(const ChunkPosition& centerPos) {
    ZoneScoped;

    size_t removed = storage_.removeOutsideRadius(centerPos, static_cast<float>(renderDistance_ + 1));
    if (removed > 0) {
        spdlog::debug("Unloaded {} chunks", removed);
    }
}

void ChunkManager::clearAllChunks() {
    ZoneScoped;

    size_t count = storage_.size();
    storage_.clear();

    // Clear work queue
    {
        std::lock_guard<std::mutex> lock(workQueueMutex_);
        while (!workQueue_.empty()) {
            workQueue_.pop();
        }
    }

    // Clear ready meshes
    {
        std::lock_guard<std::mutex> lock(readyMutex_);
        while (!readyMeshes_.empty()) {
            readyMeshes_.pop();
        }
    }

    // Clear dirty tracking
    {
        std::lock_guard<std::mutex> lock(dirtyMutex_);
        dirtyChunks_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(cameraPosMutex_);
        lastCameraChunkPos_ = {INT32_MAX, INT32_MAX, INT32_MAX};
    }

    spdlog::info("Cleared all chunks (unloaded {} chunks)", count);
}

ChunkDataPtr ChunkManager::getChunkData(const ChunkPosition& pos) const {
    return storage_.get(pos);
}

bool ChunkManager::hasChunk(const ChunkPosition& pos) const {
    return storage_.contains(pos);
}

BlockState ChunkManager::getBlockState(const glm::ivec3& worldPos) const {
    ChunkPosition chunkPos = worldToChunkPos(glm::vec3(worldPos));

    ChunkDataPtr chunk = storage_.get(chunkPos);
    if (!chunk) {
        return BlockRegistry::AIR->getDefaultState();
    }

    glm::ivec3 localPos = worldPos - glm::ivec3(
        chunkPos.x * static_cast<int32_t>(CHUNK_SIZE),
        chunkPos.y * static_cast<int32_t>(CHUNK_SIZE),
        chunkPos.z * static_cast<int32_t>(CHUNK_SIZE)
    );

    if (localPos.x < 0 || localPos.x >= static_cast<int32_t>(CHUNK_SIZE) ||
        localPos.y < 0 || localPos.y >= static_cast<int32_t>(CHUNK_SIZE) ||
        localPos.z < 0 || localPos.z >= static_cast<int32_t>(CHUNK_SIZE)) {
        return BlockRegistry::AIR->getDefaultState();
    }

    return chunk->getBlockState(localPos.x, localPos.y, localPos.z);
}

void ChunkManager::setBlockState(const glm::ivec3& worldPos, BlockState state) {
    ZoneScoped;

    ChunkPosition chunkPos = worldToChunkPos(glm::vec3(worldPos));

    ChunkDataPtr oldChunk = storage_.get(chunkPos);
    if (!oldChunk) {
        return;  // Can't set block in non-existent chunk
    }

    glm::ivec3 localPos = worldPos - glm::ivec3(
        chunkPos.x * static_cast<int32_t>(CHUNK_SIZE),
        chunkPos.y * static_cast<int32_t>(CHUNK_SIZE),
        chunkPos.z * static_cast<int32_t>(CHUNK_SIZE)
    );

    if (localPos.x < 0 || localPos.x >= static_cast<int32_t>(CHUNK_SIZE) ||
        localPos.y < 0 || localPos.y >= static_cast<int32_t>(CHUNK_SIZE) ||
        localPos.z < 0 || localPos.z >= static_cast<int32_t>(CHUNK_SIZE)) {
        return;
    }

    // Copy-on-write: create new chunk with modification
    ChunkDataPtr newChunk = oldChunk->withBlockState(localPos.x, localPos.y, localPos.z, state);

    // Atomic swap
    storage_.insert(chunkPos, newChunk);

    // Mark for remeshing
    queueChunkRemesh(chunkPos);

    // Also remesh neighbors if block is on chunk boundary
    if (localPos.x == 0) queueChunkRemesh({chunkPos.x - 1, chunkPos.y, chunkPos.z});
    if (localPos.x == static_cast<int32_t>(CHUNK_SIZE) - 1) queueChunkRemesh({chunkPos.x + 1, chunkPos.y, chunkPos.z});
    if (localPos.y == 0) queueChunkRemesh({chunkPos.x, chunkPos.y - 1, chunkPos.z});
    if (localPos.y == static_cast<int32_t>(CHUNK_SIZE) - 1) queueChunkRemesh({chunkPos.x, chunkPos.y + 1, chunkPos.z});
    if (localPos.z == 0) queueChunkRemesh({chunkPos.x, chunkPos.y, chunkPos.z - 1});
    if (localPos.z == static_cast<int32_t>(CHUNK_SIZE) - 1) queueChunkRemesh({chunkPos.x, chunkPos.y, chunkPos.z + 1});
}

void ChunkManager::meshWorker(unsigned int threadId) {
    // Set thread name for Tracy profiling
    std::string threadName = "MeshWorker " + std::to_string(threadId);
    tracy::SetThreadName(threadName.c_str());

    while (running_) {
        MeshWorkItem workItem;

        // PHASE 1: Grab work item (minimal lock time)
        {
            std::unique_lock<std::mutex> lock(workQueueMutex_);
            workQueueCV_.wait(lock, [this] { return !workQueue_.empty() || !running_; });

            if (!running_) {
                break;
            }

            if (workQueue_.empty()) {
                continue;
            }

            workItem = workQueue_.front();
            workQueue_.pop();
        }
        // Lock released - other workers can grab work NOW

        const ChunkPosition& pos = workItem.position;

        // PHASE 2: Check if chunk needs work
        ChunkDataPtr chunkData = storage_.get(pos);

        bool needsGeneration = !chunkData;
        bool needsMeshing = isDirty(pos);

        if (!needsGeneration && !needsMeshing) {
            continue;  // Nothing to do
        }

        // PHASE 3: Generate chunk if needed (NO LOCKS HELD)
        if (needsGeneration) {
            ZoneScopedN("Generate Chunk");
            chunkData = ChunkData::generate(pos);
            storage_.insert(pos, chunkData);
            markDirty(pos);
            needsMeshing = true;

            spdlog::trace("Worker {} generated chunk at ({}, {}, {})", threadId, pos.x, pos.y, pos.z);
        }

        // PHASE 4: Check if neighbors are ready for meshing
        if (needsMeshing) {
            if (!areNeighborsLoadedForMeshing(pos)) {
                // Re-queue for later
                std::lock_guard<std::mutex> lock(workQueueMutex_);
                workQueue_.push({pos, false});
                continue;
            }
        }

        // PHASE 5: Grab snapshot of chunk + neighbors (BRIEF lock per shard)
        // After this, we hold shared_ptrs - completely safe to use without locks
        std::array<ChunkDataPtr, 7> chunkWithNeighbors = storage_.getWithNeighbors(pos);
        ChunkDataPtr centerChunk = chunkWithNeighbors[0];

        if (!centerChunk) {
            continue;  // Chunk was unloaded
        }

        // Clear dirty flag BEFORE meshing (allows new dirty marks during mesh)
        clearDirty(pos);

        // PHASE 6: Generate mesh - ZERO LOCKS HELD - FULL PARALLELISM
        CompactChunkMesh mesh;
        if (!centerChunk->isEmpty()) {
            ZoneScopedN("Generate Mesh");
            mesh = generateChunkMesh(centerChunk, chunkWithNeighbors);
        } else {
            mesh.position = pos;
        }

        // PHASE 7: Push mesh to ready queue
        {
            std::lock_guard<std::mutex> lock(readyMutex_);
            readyMeshes_.push(std::move(mesh));
        }

        // PHASE 8: Queue neighbor remesh if this was a new chunk
        if (workItem.isNewChunk) {
            queueNeighborRemesh(pos);
        }
    }
}

CompactChunkMesh ChunkManager::generateChunkMesh(ChunkDataPtr chunk,
                                                  const std::array<ChunkDataPtr, 7>& neighbors) const {
    ZoneScoped;

    CompactChunkMesh mesh;
    mesh.position = chunk->getPosition();

    if (chunk->isEmpty()) {
        return mesh;
    }

    const ChunkPosition& chunkPos = chunk->getPosition();

    // Lambda to get neighbor block state (uses captured neighbor pointers)
    auto getNeighborBlockState = [&](int nx, int ny, int nz) -> BlockState {
        if (nx >= 0 && nx < static_cast<int>(CHUNK_SIZE) &&
            ny >= 0 && ny < static_cast<int>(CHUNK_SIZE) &&
            nz >= 0 && nz < static_cast<int>(CHUNK_SIZE)) {
            return chunk->getBlockState(nx, ny, nz);
        }

        // Determine which neighbor chunk
        int neighborIdx = -1;
        int localX = nx, localY = ny, localZ = nz;

        if (nx < 0) { neighborIdx = 1; localX = CHUNK_SIZE - 1; }  // West
        else if (nx >= static_cast<int>(CHUNK_SIZE)) { neighborIdx = 2; localX = 0; }  // East
        else if (ny < 0) { neighborIdx = 3; localY = CHUNK_SIZE - 1; }  // Down
        else if (ny >= static_cast<int>(CHUNK_SIZE)) { neighborIdx = 4; localY = 0; }  // Up
        else if (nz < 0) { neighborIdx = 5; localZ = CHUNK_SIZE - 1; }  // North
        else if (nz >= static_cast<int>(CHUNK_SIZE)) { neighborIdx = 6; localZ = 0; }  // South

        if (neighborIdx > 0 && neighbors[neighborIdx]) {
            return neighbors[neighborIdx]->getBlockState(localX, localY, localZ);
        }

        return BlockRegistry::AIR->getDefaultState();
    };

    // Iterate through all blocks
    for (uint32_t bx = 0; bx < CHUNK_SIZE; bx++) {
        for (uint32_t by = 0; by < CHUNK_SIZE; by++) {
            for (uint32_t bz = 0; bz < CHUNK_SIZE; bz++) {
                BlockState state = chunk->getBlockState(bx, by, bz);
                if (state.isAir()) {
                    continue;
                }

                const BlockStateVariant* variant = modelManager_.getVariantByStateId(state.id);
                const BlockModel* model = variant ? variant->model : modelManager_.getModelByStateId(state.id);

                if (!model || model->elements.empty()) {
                    continue;
                }

                int rotationX = variant ? variant->rotationX : 0;
                int rotationY = variant ? variant->rotationY : 0;

                for (const auto& element : model->elements) {
                    glm::vec3 elemFrom = element.from / 16.0f;
                    glm::vec3 elemTo = element.to / 16.0f;

                    if (rotationY != 0) {
                        elemFrom = applyYRotation(elemFrom, rotationY);
                        elemTo = applyYRotation(elemTo, rotationY);
                    }
                    if (rotationX != 0) {
                        elemFrom = applyXRotation(elemFrom, rotationX);
                        elemTo = applyXRotation(elemTo, rotationX);
                    }

                    glm::vec3 finalFrom = glm::min(elemFrom, elemTo);
                    glm::vec3 finalTo = glm::max(elemFrom, elemTo);

                    for (const auto& [faceDir, face] : element.faces) {
                        FaceDirection rotatedFaceDir = faceDir;
                        if (rotationY != 0) {
                            rotatedFaceDir = rotateYFace(rotatedFaceDir, rotationY);
                        }
                        if (rotationX != 0) {
                            rotatedFaceDir = rotateXFace(rotatedFaceDir, rotationX);
                        }

                        bool shouldRender = true;

                        if (face.cullface.has_value()) {
                            FaceDirection rotatedCullface = face.cullface.value();
                            if (rotationY != 0) {
                                rotatedCullface = rotateYFace(rotatedCullface, rotationY);
                            }
                            if (rotationX != 0) {
                                rotatedCullface = rotateXFace(rotatedCullface, rotationX);
                            }

                            if (FaceUtils::faceReachesBoundary(rotatedCullface, elemFrom, elemTo)) {
                                int faceIndex = FaceUtils::toIndex(rotatedCullface);
                                int nx = bx + FaceUtils::FACE_DIRS[faceIndex][0];
                                int ny = by + FaceUtils::FACE_DIRS[faceIndex][1];
                                int nz = bz + FaceUtils::FACE_DIRS[faceIndex][2];

                                BlockState neighborState = getNeighborBlockState(nx, ny, nz);

                                const BlockShape& currentShape = cullingSystem_.getBlockShape(state, model);
                                const BlockModel* neighborModel = modelManager_.getModelByStateId(neighborState.id);
                                const BlockShape& neighborShape = cullingSystem_.getBlockShape(neighborState, neighborModel);

                                bool shouldDrawThisFace = cullingSystem_.shouldDrawFace(
                                    state, neighborState, rotatedCullface, currentShape, neighborShape);

                                if (!shouldDrawThisFace) {
                                    shouldRender = false;
                                }
                            }
                        }

                        if (!shouldRender) {
                            continue;
                        }

                        glm::vec3 corners[4];
                        FaceUtils::getFaceVertices(rotatedFaceDir, finalFrom, finalTo, corners);

                        glm::vec2 uvs[4];
                        FaceUtils::convertUVs(face.uv, uvs);

                        uint32_t faceTextureIndex = face.textureIndex;
                        glm::vec3 normal = FaceUtils::getFaceNormal(rotatedFaceDir);

                        uint32_t quadIndex = quadLibrary_.getOrCreateQuad(normal, corners, uvs, faceTextureIndex);

                        PackedLighting lighting;
                        if (face.tintindex.has_value()) {
                            uint8_t grassR = static_cast<uint8_t>((121 * 31) / 255);
                            uint8_t grassG = static_cast<uint8_t>((192 * 31) / 255);
                            uint8_t grassB = static_cast<uint8_t>((90 * 31) / 255);
                            lighting = PackedLighting::uniform(grassR, grassG, grassB);
                        } else {
                            lighting = PackedLighting::uniform(31, 31, 31);
                        }

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

                        FaceData faceData = FaceData::pack(bx, by, bz, false, lightIndex, quadIndex);
                        mesh.faces.push_back(faceData);
                    }
                }
            }
        }
    }

    return mesh;
}

bool ChunkManager::hasReadyMeshes() const {
    std::lock_guard<std::mutex> lock(readyMutex_);
    return !readyMeshes_.empty();
}

std::vector<CompactChunkMesh> ChunkManager::getReadyMeshes() {
    ZoneScoped;
    std::vector<CompactChunkMesh> meshes;

    std::lock_guard<std::mutex> lock(readyMutex_);
    while (!readyMeshes_.empty()) {
        meshes.push_back(std::move(readyMeshes_.front()));
        readyMeshes_.pop();
    }

    return meshes;
}

void ChunkManager::queueChunkRemesh(const ChunkPosition& pos) {
    if (!storage_.contains(pos)) {
        return;
    }

    markDirty(pos);

    std::lock_guard<std::mutex> lock(workQueueMutex_);
    workQueue_.push({pos, false});
    workQueueCV_.notify_one();
}

void ChunkManager::queueNeighborRemesh(const ChunkPosition& pos) {
    std::vector<ChunkPosition> neighborsToQueue;

    for (const auto& offset : ChunkPosition::getFaceNeighborOffsets()) {
        ChunkPosition neighborPos = pos.getNeighbor(offset.x, offset.y, offset.z);
        if (storage_.contains(neighborPos)) {
            markDirty(neighborPos);
            neighborsToQueue.push_back(neighborPos);
        }
    }

    if (!neighborsToQueue.empty()) {
        std::lock_guard<std::mutex> lock(workQueueMutex_);
        for (const auto& neighborPos : neighborsToQueue) {
            workQueue_.push({neighborPos, false});
        }
        workQueueCV_.notify_all();
    }
}

bool ChunkManager::areNeighborsLoadedForMeshing(const ChunkPosition& pos) const {
    ChunkPosition cameraChunkPos;
    {
        std::lock_guard<std::mutex> lock(cameraPosMutex_);
        cameraChunkPos = lastCameraChunkPos_;
    }

    for (const auto& offset : ChunkPosition::getFaceNeighborOffsets()) {
        ChunkPosition neighborPos = pos.getNeighbor(offset.x, offset.y, offset.z);

        if (neighborPos.distanceTo(cameraChunkPos) <= renderDistance_) {
            if (!storage_.contains(neighborPos)) {
                return false;
            }
        }
    }

    return true;
}

void ChunkManager::markDirty(const ChunkPosition& pos) {
    std::lock_guard<std::mutex> lock(dirtyMutex_);
    dirtyChunks_.insert(pos);
}

bool ChunkManager::isDirty(const ChunkPosition& pos) const {
    std::lock_guard<std::mutex> lock(dirtyMutex_);
    return dirtyChunks_.find(pos) != dirtyChunks_.end();
}

void ChunkManager::clearDirty(const ChunkPosition& pos) {
    std::lock_guard<std::mutex> lock(dirtyMutex_);
    dirtyChunks_.erase(pos);
}

void ChunkManager::notifyNeighbors(const glm::ivec3& worldPos, BlockState newState) {
    const glm::ivec3 directions[] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, 0}, {0, -1, 0}
    };

    std::vector<ChunkPosition> chunksToRemesh;

    for (const auto& dir : directions) {
        glm::ivec3 neighborPos = worldPos + dir;
        BlockState neighborState = getBlockState(neighborPos);

        if (neighborState.id == BlockRegistry::AIR->getDefaultState().id) {
            continue;
        }

        Block* neighborBlock = BlockRegistry::getBlock(neighborState);

        BlockState updatedState = neighborBlock->updateShape(
            neighborState, *this, neighborPos, -dir, worldPos, newState);

        if (updatedState.id != neighborState.id) {
            setBlockState(neighborPos, updatedState);
        }
    }
}

} // namespace FarHorizon