#include "ChunkBufferManager.hpp"
#include <spdlog/spdlog.h>

namespace VoxelEngine {

void ChunkBufferManager::init(VmaAllocator allocator, size_t maxVertices, size_t maxIndices, size_t maxDrawCommands) {
    m_maxVertices = maxVertices;
    m_maxIndices = maxIndices;
    m_maxDrawCommands = maxDrawCommands;

    m_vertexBuffer.init(
        allocator,
        maxVertices * sizeof(Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    m_indexBuffer.init(
        allocator,
        maxIndices * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    m_indirectBuffer.init(
        allocator,
        maxDrawCommands * sizeof(VkDrawIndexedIndirectCommand),
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );
}

void ChunkBufferManager::cleanup() {
    m_indirectBuffer.cleanup();
    m_indexBuffer.cleanup();
    m_vertexBuffer.cleanup();
    m_meshCache.clear();
    m_allocations.clear();
}

bool ChunkBufferManager::addMeshes(std::vector<ChunkMesh>& meshes, size_t maxPerFrame) {
    if (meshes.empty()) return true;

    size_t processCount = std::min(meshes.size(), maxPerFrame);
    size_t actualProcessed = 0;

    // Map buffers once for all updates
    void* vertexData = m_vertexBuffer.map();
    void* indexData = m_indexBuffer.map();
    void* indirectData = m_indirectBuffer.map();

    for (size_t i = 0; i < processCount; i++) {
        ChunkMesh& mesh = meshes[i];
        if (mesh.indices.empty()) continue;

        // Check if we have space
        if (m_currentVertexOffset + mesh.vertices.size() > m_maxVertices ||
            m_currentIndexOffset + mesh.indices.size() > m_maxIndices ||
            m_drawCommandCount >= m_maxDrawCommands) {
            spdlog::warn("Buffer full, cannot add more meshes");
            break;
        }

        // Write vertices
        memcpy(static_cast<uint8_t*>(vertexData) + m_currentVertexOffset * sizeof(Vertex),
               mesh.vertices.data(),
               mesh.vertices.size() * sizeof(Vertex));

        // Write indices
        memcpy(static_cast<uint8_t*>(indexData) + m_currentIndexOffset * sizeof(uint32_t),
               mesh.indices.data(),
               mesh.indices.size() * sizeof(uint32_t));

        // Create draw command
        VkDrawIndexedIndirectCommand cmd{};
        cmd.indexCount = static_cast<uint32_t>(mesh.indices.size());
        cmd.instanceCount = 1;
        cmd.firstIndex = m_currentIndexOffset;
        cmd.vertexOffset = static_cast<int32_t>(m_currentVertexOffset);
        cmd.firstInstance = 0;

        memcpy(static_cast<uint8_t*>(indirectData) + m_drawCommandCount * sizeof(VkDrawIndexedIndirectCommand),
               &cmd,
               sizeof(VkDrawIndexedIndirectCommand));

        // Store allocation info
        ChunkBufferAllocation allocation;
        allocation.vertexOffset = m_currentVertexOffset;
        allocation.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
        allocation.indexOffset = m_currentIndexOffset;
        allocation.indexCount = static_cast<uint32_t>(mesh.indices.size());
        allocation.drawCommandIndex = m_drawCommandCount;

        m_allocations[mesh.position] = allocation;
        m_meshCache[mesh.position] = mesh;

        m_currentVertexOffset += allocation.vertexCount;
        m_currentIndexOffset += allocation.indexCount;
        m_drawCommandCount++;
    }

    m_vertexBuffer.unmap();
    m_indexBuffer.unmap();
    m_indirectBuffer.unmap();

    spdlog::trace("Added {} chunks to buffer ({} total)", processCount, m_meshCache.size());
    return true;
}

void ChunkBufferManager::removeUnloadedChunks(const ChunkManager& chunkManager) {
    std::vector<ChunkPosition> toRemove;

    for (const auto& [pos, allocation] : m_allocations) {
        if (!chunkManager.hasChunk(pos)) {
            toRemove.push_back(pos);
        }
    }

    if (!toRemove.empty()) {
        for (const auto& pos : toRemove) {
            m_meshCache.erase(pos);
            m_allocations.erase(pos);
        }
        spdlog::debug("Removed {} unloaded chunks from buffer", toRemove.size());
    }
}

void ChunkBufferManager::compactIfNeeded(const std::unordered_map<ChunkPosition, ChunkMesh, ChunkPositionHash>& meshCache) {
    // Calculate active space
    size_t totalActiveVertices = 0;
    size_t totalActiveIndices = 0;
    for (const auto& [pos, allocation] : m_allocations) {
        totalActiveVertices += allocation.vertexCount;
        totalActiveIndices += allocation.indexCount;
    }

    // Check if compaction is needed
    bool needsCompaction = false;
    if (m_currentVertexOffset > m_maxVertices * 0.7f || m_currentIndexOffset > m_maxIndices * 0.7f) {
        float vertexFragmentation = 1.0f - (static_cast<float>(totalActiveVertices) / m_currentVertexOffset);
        float indexFragmentation = 1.0f - (static_cast<float>(totalActiveIndices) / m_currentIndexOffset);

        if (vertexFragmentation > 0.3f || indexFragmentation > 0.3f) {
            needsCompaction = true;
            spdlog::debug("Buffer compaction needed: {:.1f}% vertex frag, {:.1f}% index frag",
                         vertexFragmentation * 100, indexFragmentation * 100);
        }
    }

    if (needsCompaction) {
        fullRebuild();
    }
}

void ChunkBufferManager::fullRebuild() {
    m_currentVertexOffset = 0;
    m_currentIndexOffset = 0;
    m_drawCommandCount = 0;

    void* vertexData = m_vertexBuffer.map();
    void* indexData = m_indexBuffer.map();
    void* indirectData = m_indirectBuffer.map();

    std::unordered_map<ChunkPosition, ChunkBufferAllocation, ChunkPositionHash> newAllocations;

    for (const auto& [pos, mesh] : m_meshCache) {
        if (mesh.indices.empty()) continue;

        // Write vertices
        memcpy(static_cast<uint8_t*>(vertexData) + m_currentVertexOffset * sizeof(Vertex),
               mesh.vertices.data(),
               mesh.vertices.size() * sizeof(Vertex));

        // Write indices
        memcpy(static_cast<uint8_t*>(indexData) + m_currentIndexOffset * sizeof(uint32_t),
               mesh.indices.data(),
               mesh.indices.size() * sizeof(uint32_t));

        // Create draw command
        VkDrawIndexedIndirectCommand cmd{};
        cmd.indexCount = static_cast<uint32_t>(mesh.indices.size());
        cmd.instanceCount = 1;
        cmd.firstIndex = m_currentIndexOffset;
        cmd.vertexOffset = static_cast<int32_t>(m_currentVertexOffset);
        cmd.firstInstance = 0;

        memcpy(static_cast<uint8_t*>(indirectData) + m_drawCommandCount * sizeof(VkDrawIndexedIndirectCommand),
               &cmd,
               sizeof(VkDrawIndexedIndirectCommand));

        // Store allocation
        ChunkBufferAllocation allocation;
        allocation.vertexOffset = m_currentVertexOffset;
        allocation.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
        allocation.indexOffset = m_currentIndexOffset;
        allocation.indexCount = static_cast<uint32_t>(mesh.indices.size());
        allocation.drawCommandIndex = m_drawCommandCount;

        newAllocations[pos] = allocation;

        m_currentVertexOffset += allocation.vertexCount;
        m_currentIndexOffset += allocation.indexCount;
        m_drawCommandCount++;
    }

    m_allocations = std::move(newAllocations);

    m_vertexBuffer.unmap();
    m_indexBuffer.unmap();
    m_indirectBuffer.unmap();

    spdlog::debug("Buffer compacted: {} chunks, {} vertices, {} indices",
                 m_meshCache.size(), m_currentVertexOffset, m_currentIndexOffset);
}

bool ChunkBufferManager::hasAllocation(const ChunkPosition& pos) const {
    return m_allocations.find(pos) != m_allocations.end();
}

} // namespace VoxelEngine