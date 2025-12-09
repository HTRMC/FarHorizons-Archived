#pragma once

#include "Chunk.hpp"
#include "ChunkData.hpp"
#include "ChunkStorage.hpp"
#include "BlockModel.hpp"
#include "FaceCullingSystem.hpp"
#include "ChunkGpuData.hpp"
#include "physics/BlockGetter.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <unordered_set>

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

/**
 * Work item for mesh generation queue.
 */
struct MeshWorkItem {
    ChunkPosition position;
    bool isNewChunk;  // True if chunk was just generated (needs neighbor remesh)
};

/**
 * High-performance chunk manager with lock-free parallel meshing.
 *
 * Architecture:
 * - ChunkStorage: Sharded concurrent storage (64 shards, shared_mutex each)
 * - Mesh workers: Grab shared_ptr snapshots, release ALL locks, mesh in parallel
 * - Zero synchronization during mesh generation (the expensive part)
 *
 * Thread safety:
 * - All public methods are thread-safe
 * - Mesh generation runs fully parallel across all cores
 * - Edits use copy-on-write (immutable ChunkData)
 */
class ChunkManager : public BlockGetter {
public:
    ChunkManager();
    ~ChunkManager();

    void initializeBlockModels();
    void preloadBlockStateModels();
    void registerTexture(const std::string& textureName, uint32_t textureIndex);
    std::vector<std::string> getRequiredTextures() const;
    void cacheTextureIndices();
    void precacheBlockShapes();

    void setRenderDistance(int32_t distance);
    int32_t getRenderDistance() const { return renderDistance_; }

    void update(const glm::vec3& cameraPosition);
    void clearAllChunks();

    ChunkPosition worldToChunkPos(const glm::vec3& worldPos) const;

    // Chunk access - returns shared_ptr for safe concurrent access
    ChunkDataPtr getChunkData(const ChunkPosition& pos) const;
    bool hasChunk(const ChunkPosition& pos) const;

    // BlockGetter interface implementation
    BlockState getBlockState(const glm::ivec3& worldPos) const override;

    // Block modification - uses copy-on-write
    void setBlockState(const glm::ivec3& worldPos, BlockState state);

    // Mesh generation (called by workers, fully parallel)
    CompactChunkMesh generateChunkMesh(ChunkDataPtr chunk,
                                        const std::array<ChunkDataPtr, 7>& neighbors) const;

    bool hasReadyMeshes() const;
    std::vector<CompactChunkMesh> getReadyMeshes();

    // Queue a chunk for remeshing
    void queueChunkRemesh(const ChunkPosition& pos);
    void queueNeighborRemesh(const ChunkPosition& pos);

    // Neighbor update system (for stairs, redstone, etc.)
    void notifyNeighbors(const glm::ivec3& worldPos, BlockState newState);

    // Get the global QuadInfo buffer (shared across all chunks)
    const std::vector<QuadInfo>& getQuadInfos() const { return quadLibrary_.getQuads(); }

    // Storage access for external iteration
    const ChunkStorage& getStorage() const { return storage_; }

private:
    // Lock-free chunk storage (64 shards)
    ChunkStorage storage_;

    int32_t renderDistance_ = 8;
    ChunkPosition lastCameraChunkPos_{INT32_MAX, INT32_MAX, INT32_MAX};
    mutable std::mutex cameraPosMutex_;
    std::atomic<bool> renderDistanceChanged_{false};

    mutable BlockModelManager modelManager_;
    mutable FaceCullingSystem cullingSystem_;
    mutable QuadInfoLibrary quadLibrary_;

    // Worker threads
    std::vector<std::thread> workerThreads_;
    std::atomic<bool> running_{true};

    // Work queue (protected by mutex, but workers release lock before heavy work)
    std::queue<MeshWorkItem> workQueue_;
    std::mutex workQueueMutex_;
    std::condition_variable workQueueCV_;

    // Ready meshes queue
    std::queue<CompactChunkMesh> readyMeshes_;
    mutable std::mutex readyMutex_;

    // Dirty tracking (which chunks need remeshing)
    std::unordered_set<ChunkPosition, ChunkPositionHash> dirtyChunks_;
    mutable std::mutex dirtyMutex_;

    void loadChunksAroundPosition(const ChunkPosition& centerPos);
    void unloadDistantChunks(const ChunkPosition& centerPos);
    void meshWorker(unsigned int threadId);

    bool areNeighborsLoadedForMeshing(const ChunkPosition& pos) const;
    void markDirty(const ChunkPosition& pos);
    bool isDirty(const ChunkPosition& pos) const;
    void clearDirty(const ChunkPosition& pos);
};

} // namespace FarHorizon