#include "ChunkManager.hpp"
#include <cmath>
#include <spdlog/spdlog.h>

namespace VoxelEngine {

static const glm::vec3 faceColors[6] = {
    glm::vec3(0.8f, 0.8f, 0.8f),
    glm::vec3(0.8f, 0.8f, 0.8f),
    glm::vec3(0.6f, 0.6f, 0.6f),
    glm::vec3(0.6f, 0.6f, 0.6f),
    glm::vec3(1.0f, 1.0f, 1.0f),
    glm::vec3(0.5f, 0.5f, 0.5f),
};

static const glm::vec3 faceNormals[6] = {
    glm::vec3(0, 0, 1),
    glm::vec3(0, 0, -1),
    glm::vec3(-1, 0, 0),
    glm::vec3(1, 0, 0),
    glm::vec3(0, 1, 0),
    glm::vec3(0, -1, 0),
};

static const int faceDirs[6][3] = {
    {0, 0, 1},
    {0, 0, -1},
    {-1, 0, 0},
    {1, 0, 0},
    {0, 1, 0},
    {0, -1, 0},
};

static const glm::vec3 faceVertices[6][4] = {
    {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
    {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}},
    {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}},
    {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}},
    {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}},
    {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},
};

static const glm::vec2 faceUVs[4] = {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {1.0f, 1.0f},
    {0.0f, 1.0f}
};

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

    for (uint32_t x = 0; x < CHUNK_SIZE; x++) {
        for (uint32_t y = 0; y < CHUNK_SIZE; y++) {
            for (uint32_t z = 0; z < CHUNK_SIZE; z++) {
                if (!chunk->getVoxel(x, y, z)) {
                    continue;
                }

                glm::vec3 voxelPos(x, y, z);

                for (int face = 0; face < 6; face++) {
                    int nx = x + faceDirs[face][0];
                    int ny = y + faceDirs[face][1];
                    int nz = z + faceDirs[face][2];

                    bool shouldRenderFace = false;

                    if (nx < 0 || nx >= CHUNK_SIZE || ny < 0 || ny >= CHUNK_SIZE || nz < 0 || nz >= CHUNK_SIZE) {
                        // Face is on chunk boundary - check neighbor chunk
                        ChunkPosition neighborChunkPos = chunkPos;
                        int localX = nx;
                        int localY = ny;
                        int localZ = nz;

                        if (nx < 0) { neighborChunkPos.x--; localX = CHUNK_SIZE - 1; }
                        else if (nx >= CHUNK_SIZE) { neighborChunkPos.x++; localX = 0; }

                        if (ny < 0) { neighborChunkPos.y--; localY = CHUNK_SIZE - 1; }
                        else if (ny >= CHUNK_SIZE) { neighborChunkPos.y++; localY = 0; }

                        if (nz < 0) { neighborChunkPos.z--; localZ = CHUNK_SIZE - 1; }
                        else if (nz >= CHUNK_SIZE) { neighborChunkPos.z++; localZ = 0; }

                        const Chunk* neighborChunk = getChunk(neighborChunkPos);
                        if (!neighborChunk || !neighborChunk->getVoxel(localX, localY, localZ)) {
                            shouldRenderFace = true;
                        }
                    } else {
                        if (!chunk->getVoxel(nx, ny, nz)) {
                            shouldRenderFace = true;
                        }
                    }

                    if (shouldRenderFace) {
                        uint32_t baseVertex = static_cast<uint32_t>(mesh.vertices.size());

                        for (int i = 0; i < 4; i++) {
                            Vertex vertex;
                            vertex.position = chunkWorldPos + voxelPos + faceVertices[face][i];
                            vertex.color = faceColors[face];
                            vertex.texCoord = faceUVs[i];
                            vertex.textureIndex = textureIndex;
                            mesh.vertices.push_back(vertex);
                        }

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