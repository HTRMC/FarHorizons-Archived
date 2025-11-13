#pragma once

#include "Buffer.hpp"
#include "../../world/ChunkManager.hpp"
#include "../../world/ChunkGpuData.hpp"
#include <unordered_map>
#include <vector>

namespace FarHorizon {

struct ChunkBufferAllocation {
    uint32_t faceOffset;      // Offset in FaceData buffer
    uint32_t faceCount;       // Number of faces
    uint32_t lightingOffset;  // Offset in lighting buffer
    uint32_t drawCommandIndex;
};

class ChunkBufferManager {
public:
    void init(VmaAllocator allocator, size_t maxFaces, size_t maxDrawCommands);
    void cleanup();
    void clear();  // Clear all meshes and reset state

    // Add new meshes incrementally (returns false if buffer is full)
    bool addMeshes(std::vector<CompactChunkMesh>& meshes, size_t maxPerFrame);

    // Remove meshes for unloaded chunks
    void removeUnloadedChunks(const ChunkManager& chunkManager);

    // Compact buffer when fragmented
    void compactIfNeeded(const std::unordered_map<ChunkPosition, CompactChunkMesh, ChunkPositionHash>& meshCache);

    // Get current draw count for rendering
    uint32_t getDrawCommandCount() const { return drawCommandCount_; }

    // Get buffers for binding
    VkBuffer getFaceBuffer() const { return faceBuffer_.getBuffer(); }
    VkBuffer getLightingBuffer() const { return lightingBuffer_.getBuffer(); }
    VkBuffer getIndirectBuffer() const { return indirectBuffer_.getBuffer(); }
    VkBuffer getChunkDataBuffer() const { return chunkDataBuffer_.getBuffer(); }

    // Check if a chunk has an allocation
    bool hasAllocation(const ChunkPosition& pos) const;

    // Get mesh cache (meshes currently in buffers)
    const std::unordered_map<ChunkPosition, CompactChunkMesh, ChunkPositionHash>& getMeshCache() const { return meshCache_; }
    std::unordered_map<ChunkPosition, CompactChunkMesh, ChunkPositionHash>& getMeshCache() { return meshCache_; }

private:
    Buffer faceBuffer_;      // FaceData buffer (replaces vertex buffer)
    Buffer lightingBuffer_;  // PackedLighting buffer (replaces index buffer)
    Buffer indirectBuffer_;  // VkDrawIndirectCommand buffer
    Buffer chunkDataBuffer_; // ChunkData buffer (per-chunk metadata, indexed by gl_BaseInstance)

    size_t maxFaces_;
    size_t maxDrawCommands_;

    uint32_t currentFaceOffset_ = 0;
    uint32_t currentLightingOffset_ = 0;
    uint32_t drawCommandCount_ = 0;

    std::unordered_map<ChunkPosition, CompactChunkMesh, ChunkPositionHash> meshCache_;
    std::unordered_map<ChunkPosition, ChunkBufferAllocation, ChunkPositionHash> allocations_;
    std::vector<ChunkData> chunkDataArray_;  // CPU-side copy of chunk data (indexed by draw command)

    void fullRebuild();
    void rebuildDrawCommands();  // Fast rebuild: only updates draw commands, not face/lighting data
};

} // namespace FarHorizon