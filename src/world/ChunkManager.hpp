#pragma once

#include "Chunk.hpp"
#include "BlockModel.hpp"
#include "FaceCullingSystem.hpp"
#include "ChunkGpuData.hpp"
#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <condition_variable>

namespace FarHorizon {

/**
 * Manages a library of unique quad geometries.
 * Multiple faces can reference the same QuadInfo to save memory.
 */
class QuadInfoLibrary {
public:
    /**
     * Get or create a QuadInfo index for the given geometry.
     * Returns the index in the QuadInfo buffer.
     */
    uint32_t getOrCreateQuad(const glm::vec3& normal,
                             const glm::vec3 corners[4],
                             const glm::vec2 uvs[4],
                             uint32_t textureSlot);

    const std::vector<QuadInfo>& getQuads() const { return m_quads; }
    void clear() { m_quads.clear(); m_quadMap.clear(); }

private:
    struct QuadKey {
        glm::vec3 normal;
        glm::vec3 corners[4];
        glm::vec2 uvs[4];
        uint32_t textureSlot;

        bool operator==(const QuadKey& other) const;
    };

    struct QuadKeyHash {
        size_t operator()(const QuadKey& key) const;
    };

    std::vector<QuadInfo> m_quads;
    std::unordered_map<QuadKey, uint32_t, QuadKeyHash> m_quadMap;
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
    void precacheBlockShapes();  // Pre-compute all BlockShapes (call after models are loaded)

    void setRenderDistance(int32_t distance);
    int32_t getRenderDistance() const { return m_renderDistance; }

    void update(const glm::vec3& cameraPosition);
    void clearAllChunks();

    ChunkPosition worldToChunkPos(const glm::vec3& worldPos) const;

    Chunk* getChunk(const ChunkPosition& pos);
    const Chunk* getChunk(const ChunkPosition& pos) const;
    bool hasChunk(const ChunkPosition& pos) const;

    const std::unordered_map<ChunkPosition, std::unique_ptr<Chunk>, ChunkPositionHash>& getChunks() const {
        return m_chunks;
    }

    CompactChunkMesh generateChunkMesh(const Chunk* chunk) const;

    bool hasReadyMeshes() const;
    std::vector<CompactChunkMesh> getReadyMeshes();

    // Queue a chunk for remeshing (e.g., when blocks change)
    void queueChunkRemesh(const ChunkPosition& pos);

    // Queue neighbor chunks for remeshing (e.g., when blocks change at chunk boundaries)
    void queueNeighborRemesh(const ChunkPosition& pos);

    // Get the global QuadInfo buffer (shared across all chunks)
    const std::vector<QuadInfo>& getQuadInfos() const { return m_quadLibrary.getQuads(); }

private:
    std::unordered_map<ChunkPosition, std::unique_ptr<Chunk>, ChunkPositionHash> m_chunks;
    int32_t m_renderDistance = 8;
    ChunkPosition m_lastCameraChunkPos = {INT32_MAX, INT32_MAX, INT32_MAX};
    bool m_renderDistanceChanged = false;

    mutable BlockModelManager m_modelManager;
    mutable FaceCullingSystem m_cullingSystem;  // Face culling with Minecraft-style fast paths
    mutable QuadInfoLibrary m_quadLibrary;  // Shared quad geometry library

    std::vector<std::thread> m_workerThreads;
    std::queue<ChunkPosition> m_meshQueue;
    std::queue<CompactChunkMesh> m_readyMeshes;
    mutable std::mutex m_chunksMutex;
    std::mutex m_queueMutex;
    mutable std::mutex m_readyMutex;
    std::condition_variable m_queueCV;
    std::atomic<bool> m_running{true};

    void loadChunksAroundPosition(const ChunkPosition& centerPos);
    void unloadDistantChunks(const ChunkPosition& centerPos);
    Chunk* loadChunk(const ChunkPosition& pos);
    void meshWorker();

    // Helper: Check if all required neighbors are loaded for meshing (Minecraft's ChunkRegion approach)
    bool areNeighborsLoadedForMeshing(const ChunkPosition& pos) const;
};

} // namespace FarHorizon