#pragma once

#include "Chunk.hpp"
#include "BlockModel.hpp"
#include "FaceCullingSystem.hpp"
#include "ChunkGpuData.hpp"
#include "physics/BlockGetter.hpp"
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

    const std::vector<QuadInfo>& getQuads() const { return quads_; }
    void clear() { quads_.clear(); quadMap_.clear(); }

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

    std::vector<QuadInfo> quads_;
    std::unordered_map<QuadKey, uint32_t, QuadKeyHash> quadMap_;
};

class ChunkManager : public BlockGetter {
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
    int32_t getRenderDistance() const { return renderDistance_; }

    void update(const glm::vec3& cameraPosition);
    void clearAllChunks();

    ChunkPosition worldToChunkPos(const glm::vec3& worldPos) const;

    Chunk* getChunk(const ChunkPosition& pos);
    const Chunk* getChunk(const ChunkPosition& pos) const;
    bool hasChunk(const ChunkPosition& pos) const;

    const std::unordered_map<ChunkPosition, std::unique_ptr<Chunk>, ChunkPositionHash>& getChunks() const {
        return chunks_;
    }

    // BlockGetter interface implementation
    BlockState getBlockState(const glm::ivec3& worldPos) const override;

    CompactChunkMesh generateChunkMesh(const Chunk* chunk) const;

    bool hasReadyMeshes() const;
    std::vector<CompactChunkMesh> getReadyMeshes();

    // Queue a chunk for remeshing (e.g., when blocks change)
    void queueChunkRemesh(const ChunkPosition& pos);

    // Queue neighbor chunks for remeshing (e.g., when blocks change at chunk boundaries)
    void queueNeighborRemesh(const ChunkPosition& pos);

    // Notify all neighboring blocks that a block has changed (Minecraft's neighbor update system)
    // Used for stairs, doors, redstone, etc. to update their state when neighbors change
    void notifyNeighbors(const glm::ivec3& worldPos, BlockState newState);

    // Get the global QuadInfo buffer (shared across all chunks)
    const std::vector<QuadInfo>& getQuadInfos() const { return quadLibrary_.getQuads(); }

private:
    std::unordered_map<ChunkPosition, std::unique_ptr<Chunk>, ChunkPositionHash> chunks_;
    int32_t renderDistance_ = 8;
    ChunkPosition lastCameraChunkPos_ = {INT32_MAX, INT32_MAX, INT32_MAX};
    bool renderDistanceChanged_ = false;

    mutable BlockModelManager modelManager_;
    mutable FaceCullingSystem cullingSystem_;  // Face culling system
    mutable QuadInfoLibrary quadLibrary_;  // Shared quad geometry library

    std::vector<std::thread> workerThreads_;
    std::queue<ChunkPosition> meshQueue_;
    std::queue<CompactChunkMesh> readyMeshes_;
    mutable std::mutex chunksMutex_;
    std::mutex queueMutex_;
    mutable std::mutex readyMutex_;
    std::condition_variable queueCV_;
    std::atomic<bool> running_{true};

    void loadChunksAroundPosition(const ChunkPosition& centerPos);
    void unloadDistantChunks(const ChunkPosition& centerPos);
    Chunk* loadChunk(const ChunkPosition& pos);
    void meshWorker();

    // Helper: Check if all required neighbors are loaded for meshing
    bool areNeighborsLoadedForMeshing(const ChunkPosition& pos) const;
};

} // namespace FarHorizon