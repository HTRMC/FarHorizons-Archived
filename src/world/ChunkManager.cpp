#include "ChunkManager.hpp"
#include <cmath>
#include <iostream>

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
        m_lastCameraChunkPos = cameraChunkPos;
    }
}

void ChunkManager::loadChunksAroundPosition(const ChunkPosition& centerPos) {
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
                        loadChunk(pos);
                    }
                }
            }
        }
    }
}

Chunk* ChunkManager::loadChunk(const ChunkPosition& pos) {
    auto chunk = std::make_unique<Chunk>(pos);
    chunk->generate();

    Chunk* chunkPtr = chunk.get();
    m_chunks[pos] = std::move(chunk);
    m_chunksChanged = true;

    if (!chunkPtr->isEmpty()) {
        std::cout << "[ChunkManager] Loaded chunk at (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
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

    return mesh;
}

} // namespace VoxelEngine