#pragma once

#include "Buffer.hpp"
#include "../../world/ChunkManager.hpp"
#include <unordered_map>
#include <vector>

namespace VoxelEngine {

struct ChunkBufferAllocation {
    uint32_t vertexOffset;
    uint32_t vertexCount;
    uint32_t indexOffset;
    uint32_t indexCount;
    uint32_t drawCommandIndex;
};

class ChunkBufferManager {
public:
    void init(VmaAllocator allocator, size_t maxVertices, size_t maxIndices, size_t maxDrawCommands);
    void cleanup();

    // Add new meshes incrementally (returns false if buffer is full)
    bool addMeshes(std::vector<ChunkMesh>& meshes, size_t maxPerFrame);

    // Remove meshes for unloaded chunks
    void removeUnloadedChunks(const ChunkManager& chunkManager);

    // Compact buffer when fragmented
    void compactIfNeeded(const std::unordered_map<ChunkPosition, ChunkMesh, ChunkPositionHash>& meshCache);

    // Get current draw count for rendering
    uint32_t getDrawCommandCount() const { return m_drawCommandCount; }

    // Get buffers for binding
    VkBuffer getVertexBuffer() const { return m_vertexBuffer.getBuffer(); }
    VkBuffer getIndexBuffer() const { return m_indexBuffer.getBuffer(); }
    VkBuffer getIndirectBuffer() const { return m_indirectBuffer.getBuffer(); }

    // Check if a chunk has an allocation
    bool hasAllocation(const ChunkPosition& pos) const;

    // Get mesh cache (meshes currently in buffers)
    const std::unordered_map<ChunkPosition, ChunkMesh, ChunkPositionHash>& getMeshCache() const { return m_meshCache; }
    std::unordered_map<ChunkPosition, ChunkMesh, ChunkPositionHash>& getMeshCache() { return m_meshCache; }

private:
    Buffer m_vertexBuffer;
    Buffer m_indexBuffer;
    Buffer m_indirectBuffer;

    size_t m_maxVertices;
    size_t m_maxIndices;
    size_t m_maxDrawCommands;

    uint32_t m_currentVertexOffset = 0;
    uint32_t m_currentIndexOffset = 0;
    uint32_t m_drawCommandCount = 0;

    std::unordered_map<ChunkPosition, ChunkMesh, ChunkPositionHash> m_meshCache;
    std::unordered_map<ChunkPosition, ChunkBufferAllocation, ChunkPositionHash> m_allocations;

    void fullRebuild();
};

} // namespace VoxelEngine