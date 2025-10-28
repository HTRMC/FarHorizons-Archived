#pragma once

#include "Chunk.hpp"
#include "BlockModel.hpp"
#include "FaceCullingSystem.hpp"
#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <condition_variable>

namespace VoxelEngine {

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 texCoord;
    uint32_t textureIndex;
};

struct ChunkMesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    ChunkPosition position;
};

class ChunkManager {
public:
    ChunkManager();
    ~ChunkManager();

    void initializeBlockModels();
    void preloadBlockStateModels();  // Preload all blockstate models into cache
    void registerTexture(const std::string& textureName, uint32_t textureIndex);
    std::vector<std::string> getRequiredTextures() const;
    void cacheTextureIndices();  // Cache texture indices in models (call after texture registration)

    void setRenderDistance(int32_t distance) { m_renderDistance = distance; }
    int32_t getRenderDistance() const { return m_renderDistance; }

    void update(const glm::vec3& cameraPosition);

    ChunkPosition worldToChunkPos(const glm::vec3& worldPos) const;

    Chunk* getChunk(const ChunkPosition& pos);
    const Chunk* getChunk(const ChunkPosition& pos) const;
    bool hasChunk(const ChunkPosition& pos) const;

    const std::unordered_map<ChunkPosition, std::unique_ptr<Chunk>, ChunkPositionHash>& getChunks() const {
        return m_chunks;
    }

    ChunkMesh generateChunkMesh(const Chunk* chunk, uint32_t textureIndex) const;

    bool hasReadyMeshes() const;
    std::vector<ChunkMesh> getReadyMeshes();

    // Queue a chunk for remeshing (e.g., when blocks change)
    void queueChunkRemesh(const ChunkPosition& pos);

private:
    std::unordered_map<ChunkPosition, std::unique_ptr<Chunk>, ChunkPositionHash> m_chunks;
    int32_t m_renderDistance = 8;
    ChunkPosition m_lastCameraChunkPos = {INT32_MAX, INT32_MAX, INT32_MAX};

    mutable BlockModelManager m_modelManager;
    mutable FaceCullingSystem m_cullingSystem;  // Face culling with Minecraft-style fast paths

    std::vector<std::thread> m_workerThreads;
    std::queue<ChunkPosition> m_meshQueue;
    std::queue<ChunkMesh> m_readyMeshes;
    mutable std::mutex m_chunksMutex;
    std::mutex m_queueMutex;
    mutable std::mutex m_readyMutex;
    std::condition_variable m_queueCV;
    std::atomic<bool> m_running{true};

    void loadChunksAroundPosition(const ChunkPosition& centerPos);
    void unloadDistantChunks(const ChunkPosition& centerPos);
    Chunk* loadChunk(const ChunkPosition& pos);
    void meshWorker();
    void queueNeighborRemesh(const ChunkPosition& pos);
};

} // namespace VoxelEngine