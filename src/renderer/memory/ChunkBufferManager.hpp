#pragma once

#include "Buffer.hpp"
#include "../../world/ChunkManager.hpp"
#include "../../world/ChunkGpuData.hpp"
#include <unordered_map>
#include <vector>

namespace VoxelEngine {

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

    // Add new meshes incrementally (returns false if buffer is full)
    bool addMeshes(std::vector<CompactChunkMesh>& meshes, size_t maxPerFrame);

    // Remove meshes for unloaded chunks
    void removeUnloadedChunks(const ChunkManager& chunkManager);

    // Compact buffer when fragmented
    void compactIfNeeded(const std::unordered_map<ChunkPosition, CompactChunkMesh, ChunkPositionHash>& meshCache);

    // Get current draw count for rendering
    uint32_t getDrawCommandCount() const { return m_drawCommandCount; }

    // Get buffers for binding
    VkBuffer getFaceBuffer() const { return m_faceBuffer.getBuffer(); }
    VkBuffer getLightingBuffer() const { return m_lightingBuffer.getBuffer(); }
    VkBuffer getIndirectBuffer() const { return m_indirectBuffer.getBuffer(); }
    VkBuffer getChunkDataBuffer() const { return m_chunkDataBuffer.getBuffer(); }

    // Check if a chunk has an allocation
    bool hasAllocation(const ChunkPosition& pos) const;

    // Get mesh cache (meshes currently in buffers)
    const std::unordered_map<ChunkPosition, CompactChunkMesh, ChunkPositionHash>& getMeshCache() const { return m_meshCache; }
    std::unordered_map<ChunkPosition, CompactChunkMesh, ChunkPositionHash>& getMeshCache() { return m_meshCache; }

private:
    Buffer m_faceBuffer;      // FaceData buffer (replaces vertex buffer)
    Buffer m_lightingBuffer;  // PackedLighting buffer (replaces index buffer)
    Buffer m_indirectBuffer;  // VkDrawIndirectCommand buffer
    Buffer m_chunkDataBuffer; // ChunkData buffer (per-chunk metadata, indexed by gl_BaseInstance)

    size_t m_maxFaces;
    size_t m_maxDrawCommands;

    uint32_t m_currentFaceOffset = 0;
    uint32_t m_currentLightingOffset = 0;
    uint32_t m_drawCommandCount = 0;

    std::unordered_map<ChunkPosition, CompactChunkMesh, ChunkPositionHash> m_meshCache;
    std::unordered_map<ChunkPosition, ChunkBufferAllocation, ChunkPositionHash> m_allocations;
    std::vector<ChunkData> m_chunkDataArray;  // CPU-side copy of chunk data (indexed by draw command)

    void fullRebuild();
};

} // namespace VoxelEngine