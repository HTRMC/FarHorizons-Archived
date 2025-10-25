#include "ChunkManager.hpp"
#include "FaceUtils.hpp"
#include <cmath>
#include <spdlog/spdlog.h>

namespace VoxelEngine {

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

void ChunkManager::initializeBlockModels(const std::string& modelsPath) {
    m_modelManager.initialize(modelsPath);
}

void ChunkManager::registerTexture(const std::string& textureName, uint32_t textureIndex) {
    m_modelManager.registerTexture(textureName, textureIndex);
}

std::vector<std::string> ChunkManager::getRequiredTextures() const {
    return m_modelManager.getAllTextureNames();
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
            int32_t dx = pos.x - centerPos.x;
            int32_t dy = pos.y - centerPos.y;
            int32_t dz = pos.z - centerPos.z;
            float distance = std::sqrt(dx*dx + dy*dy + dz*dz);

            if (distance > m_renderDistance + 1) {
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

ChunkMesh ChunkManager::generateChunkMesh(const Chunk* chunk, uint32_t textureIndex) const {
    ChunkMesh mesh;

    if (chunk->isEmpty()) {
        return mesh;
    }

    const ChunkPosition& chunkPos = chunk->getPosition();
    glm::vec3 chunkWorldPos(
        chunkPos.x * static_cast<int32_t>(CHUNK_SIZE),
        chunkPos.y * static_cast<int32_t>(CHUNK_SIZE),
        chunkPos.z * static_cast<int32_t>(CHUNK_SIZE)
    );

    // Iterate through all blocks in the chunk
    for (uint32_t bx = 0; bx < CHUNK_SIZE; bx++) {
        for (uint32_t by = 0; by < CHUNK_SIZE; by++) {
            for (uint32_t bz = 0; bz < CHUNK_SIZE; bz++) {
                BlockType blockType = chunk->getBlock(bx, by, bz);
                if (blockType == BlockType::AIR) {
                    continue;
                }

                // Get the block model
                const BlockModel* model = m_modelManager.getModel(blockType);
                if (!model || model->elements.empty()) {
                    // Fallback to simple cube if no model found
                    spdlog::warn("No model found for block type: {}", static_cast<int>(blockType));
                    continue;
                }

                glm::vec3 blockPos(bx, by, bz);
                glm::vec3 blockWorldPos = chunkWorldPos + blockPos;

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
                            // Get the direction to check
                            int faceIndex = FaceUtils::toIndex(face.cullface.value());
                            int nx = bx + FaceUtils::FACE_DIRS[faceIndex][0];
                            int ny = by + FaceUtils::FACE_DIRS[faceIndex][1];
                            int nz = bz + FaceUtils::FACE_DIRS[faceIndex][2];

                            // Check if neighbor is solid
                            if (nx < 0 || nx >= CHUNK_SIZE || ny < 0 || ny >= CHUNK_SIZE || nz < 0 || nz >= CHUNK_SIZE) {
                                // Check neighbor chunk
                                ChunkPosition neighborChunkPos = chunkPos;
                                int localX = nx, localY = ny, localZ = nz;

                                if (nx < 0) { neighborChunkPos.x--; localX = CHUNK_SIZE - 1; }
                                else if (nx >= CHUNK_SIZE) { neighborChunkPos.x++; localX = 0; }
                                if (ny < 0) { neighborChunkPos.y--; localY = CHUNK_SIZE - 1; }
                                else if (ny >= CHUNK_SIZE) { neighborChunkPos.y++; localY = 0; }
                                if (nz < 0) { neighborChunkPos.z--; localZ = CHUNK_SIZE - 1; }
                                else if (nz >= CHUNK_SIZE) { neighborChunkPos.z++; localZ = 0; }

                                const Chunk* neighborChunk = getChunk(neighborChunkPos);
                                if (neighborChunk && isBlockSolid(neighborChunk->getBlock(localX, localY, localZ))) {
                                    shouldRender = false;  // Face is culled
                                }
                            } else {
                                // Check within chunk
                                if (isBlockSolid(chunk->getBlock(nx, ny, nz))) {
                                    shouldRender = false;  // Face is culled
                                }
                            }
                        }

                        if (!shouldRender) {
                            continue;
                        }

                        // Generate vertices for this face
                        glm::vec3 vertices[4];
                        FaceUtils::getFaceVertices(faceDir, elemFrom, elemTo, vertices);
                        int faceIndex = FaceUtils::toIndex(faceDir);

                        // Convert UVs from Blockbench format to Vulkan format
                        glm::vec2 uvs[4];
                        FaceUtils::convertUVs(face.uv, uvs);

                        // Resolve texture from model and get its index
                        std::string resolvedTexture = model->resolveTexture(face.texture);
                        uint32_t faceTextureIndex = m_modelManager.getTextureIndex(resolvedTexture);

                        // Add vertices
                        uint32_t baseVertex = static_cast<uint32_t>(mesh.vertices.size());
                        for (int i = 0; i < 4; i++) {
                            Vertex vertex;
                            vertex.position = blockWorldPos + vertices[i];
                            vertex.color = FaceUtils::FACE_COLORS[faceIndex];
                            vertex.texCoord = uvs[i];
                            vertex.textureIndex = faceTextureIndex;
                            mesh.vertices.push_back(vertex);
                        }

                        // Add indices (two triangles per face)
                        mesh.indices.push_back(baseVertex + 0);
                        mesh.indices.push_back(baseVertex + 1);
                        mesh.indices.push_back(baseVertex + 2);
                        mesh.indices.push_back(baseVertex + 2);
                        mesh.indices.push_back(baseVertex + 3);
                        mesh.indices.push_back(baseVertex + 0);
                    }
                }
            }
        }
    }

    mesh.position = chunkPos;
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
        if (!chunkExists) {
            auto chunk = std::make_unique<Chunk>(pos);
            chunk->generate();

            std::lock_guard<std::mutex> lock(m_chunksMutex);
            // Double-check it wasn't added by another thread
            if (m_chunks.find(pos) == m_chunks.end()) {
                m_chunks[pos] = std::move(chunk);
                wasNewlyCreated = true;
                spdlog::trace("Worker generated chunk at ({}, {}, {})", pos.x, pos.y, pos.z);
            }
        }

        // Now generate the mesh
        ChunkMesh mesh;
        {
            std::lock_guard<std::mutex> lock(m_chunksMutex);
            const Chunk* chunk = getChunk(pos);
            if (chunk && !chunk->isEmpty()) {
                mesh = generateChunkMesh(chunk, 0);
            }
        }

        if (!mesh.indices.empty()) {
            std::lock_guard<std::mutex> lock(m_readyMutex);
            m_readyMeshes.push(std::move(mesh));
        }

        // If this was a new chunk, queue neighbors for remeshing to update boundary faces
        if (wasNewlyCreated) {
            queueNeighborRemesh(pos);
        }
    }
}

bool ChunkManager::hasReadyMeshes() const {
    std::lock_guard<std::mutex> lock(m_readyMutex);
    return !m_readyMeshes.empty();
}

std::vector<ChunkMesh> ChunkManager::getReadyMeshes() {
    std::vector<ChunkMesh> meshes;

    std::lock_guard<std::mutex> lock(m_readyMutex);
    while (!m_readyMeshes.empty()) {
        meshes.push_back(std::move(m_readyMeshes.front()));
        m_readyMeshes.pop();
    }

    return meshes;
}

void ChunkManager::queueNeighborRemesh(const ChunkPosition& pos) {
    std::lock_guard<std::mutex> lock(m_queueMutex);

    // Queue the 6 face-adjacent neighbors for remeshing
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dz = -1; dz <= 1; dz++) {
                // Skip self and diagonals (only faces, not edges/corners)
                if (abs(dx) + abs(dy) + abs(dz) != 1) continue;

                ChunkPosition neighborPos = {pos.x + dx, pos.y + dy, pos.z + dz};

                // Check if neighbor chunk exists
                bool neighborExists = false;
                {
                    std::lock_guard<std::mutex> chunkLock(m_chunksMutex);
                    neighborExists = (m_chunks.find(neighborPos) != m_chunks.end());
                }

                if (neighborExists) {
                    m_meshQueue.push(neighborPos);
                }
            }
        }
    }

    m_queueCV.notify_all();
}

} // namespace VoxelEngine