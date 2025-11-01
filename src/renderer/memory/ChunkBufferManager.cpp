#include "ChunkBufferManager.hpp"
#include <spdlog/spdlog.h>

namespace VoxelEngine {

void ChunkBufferManager::init(VmaAllocator allocator, size_t maxFaces, size_t maxDrawCommands) {
    m_maxFaces = maxFaces;
    m_maxDrawCommands = maxDrawCommands;

    // FaceData buffer (8 bytes per face, used as SSBO)
    m_faceBuffer.init(
        allocator,
        maxFaces * sizeof(FaceData),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    // Lighting buffer (16 bytes per face)
    m_lightingBuffer.init(
        allocator,
        maxFaces * sizeof(PackedLighting),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    // Indirect draw buffer (non-indexed instanced drawing)
    m_indirectBuffer.init(
        allocator,
        maxDrawCommands * sizeof(VkDrawIndirectCommand),
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    // ChunkData buffer (per-chunk metadata, indexed by gl_BaseInstance)
    m_chunkDataBuffer.init(
        allocator,
        maxDrawCommands * sizeof(ChunkData),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    // Reserve space for chunk data array
    m_chunkDataArray.reserve(maxDrawCommands);

    spdlog::info("ChunkBufferManager initialized: {} max faces, {} max draw commands", maxFaces, maxDrawCommands);
}

void ChunkBufferManager::cleanup() {
    m_chunkDataBuffer.cleanup();
    m_indirectBuffer.cleanup();
    m_lightingBuffer.cleanup();
    m_faceBuffer.cleanup();
    m_meshCache.clear();
    m_allocations.clear();
    m_chunkDataArray.clear();
}

void ChunkBufferManager::clear() {
    m_meshCache.clear();
    m_allocations.clear();
    m_chunkDataArray.clear();
    m_currentFaceOffset = 0;
    m_currentLightingOffset = 0;
    m_drawCommandCount = 0;
    spdlog::info("Cleared all chunk meshes from GPU buffers");
}

bool ChunkBufferManager::addMeshes(std::vector<CompactChunkMesh>& meshes, size_t maxPerFrame) {
    if (meshes.empty()) return true;

    size_t processCount = std::min(meshes.size(), maxPerFrame);
    size_t actualProcessed = 0;

    // Map buffers once for all updates
    void* faceData = m_faceBuffer.map();
    void* lightingData = m_lightingBuffer.map();
    void* indirectData = m_indirectBuffer.map();
    void* chunkData = m_chunkDataBuffer.map();

    for (size_t i = 0; i < processCount; i++) {
        CompactChunkMesh& mesh = meshes[i];
        if (mesh.faces.empty()) continue;

        // Check if we have space
        if (m_currentFaceOffset + mesh.faces.size() > m_maxFaces ||
            m_drawCommandCount >= m_maxDrawCommands) {
            spdlog::warn("Buffer full, cannot add more meshes");
            break;
        }

        // Write FaceData: copy then adjust lightIndex from local to global
        FaceData* destFaces = static_cast<FaceData*>(faceData) + m_currentFaceOffset;
        memcpy(destFaces, mesh.faces.data(), mesh.faces.size() * sizeof(FaceData));

        // Adjust lightIndex from local (chunk-relative) to global (buffer-relative)
        for (size_t j = 0; j < mesh.faces.size(); j++) {
            uint32_t localLightIndex = (destFaces[j].packed1 >> 16) & 0xFFFF;
            uint32_t globalLightIndex = m_currentLightingOffset + localLightIndex;
            destFaces[j].packed1 = (destFaces[j].packed1 & 0xFFFF) | (globalLightIndex << 16);
        }

        // Write lighting data
        memcpy(static_cast<uint8_t*>(lightingData) + m_currentLightingOffset * sizeof(PackedLighting),
               mesh.lighting.data(),
               mesh.lighting.size() * sizeof(PackedLighting));

        // Create and store ChunkData (indexed by gl_BaseInstance = drawCommandIndex)
        // faceOffset == lightingOffset since they're both incremented by the same amount
        ChunkData chunkMetadata = ChunkData::create(mesh.position, m_currentFaceOffset);
        m_chunkDataArray.push_back(chunkMetadata);
        memcpy(static_cast<uint8_t*>(chunkData) + m_drawCommandCount * sizeof(ChunkData),
               &chunkMetadata,
               sizeof(ChunkData));

        // Create draw command (instanced non-indexed: 6 vertices per face)
        VkDrawIndirectCommand cmd{};
        cmd.vertexCount = 6;  // 2 triangles per quad (6 vertices total)
        cmd.instanceCount = static_cast<uint32_t>(mesh.faces.size());  // One instance per face
        cmd.firstVertex = 0;
        cmd.firstInstance = m_drawCommandCount;  // Chunk ID for gl_BaseInstance (indexes into ChunkData buffer)

        memcpy(static_cast<uint8_t*>(indirectData) + m_drawCommandCount * sizeof(VkDrawIndirectCommand),
               &cmd,
               sizeof(VkDrawIndirectCommand));

        // Store allocation info
        ChunkBufferAllocation allocation;
        allocation.faceOffset = m_currentFaceOffset;
        allocation.faceCount = static_cast<uint32_t>(mesh.faces.size());
        allocation.lightingOffset = m_currentLightingOffset;
        allocation.drawCommandIndex = m_drawCommandCount;

        m_allocations[mesh.position] = allocation;

        // Important: increment lighting offset by number of unique lighting values, not faces!
        uint32_t lightingCount = static_cast<uint32_t>(mesh.lighting.size());
        m_currentFaceOffset += allocation.faceCount;
        m_currentLightingOffset += lightingCount;  // Number of unique lighting values
        m_drawCommandCount++;
        actualProcessed++;

        m_meshCache[mesh.position] = std::move(mesh);
    }

    m_lightingBuffer.unmap();
    m_chunkDataBuffer.unmap();
    m_faceBuffer.unmap();
    m_indirectBuffer.unmap();

    spdlog::trace("Added {} chunks to buffer ({} total)", actualProcessed, m_meshCache.size());
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

        // Rebuild all buffers to reflect removed chunks
        fullRebuild();
    }
}

void ChunkBufferManager::compactIfNeeded(const std::unordered_map<ChunkPosition, CompactChunkMesh, ChunkPositionHash>& meshCache) {
    // Calculate active space
    size_t totalActiveFaces = 0;
    for (const auto& [pos, allocation] : m_allocations) {
        totalActiveFaces += allocation.faceCount;
    }

    // Check if compaction is needed
    bool needsCompaction = false;
    if (m_currentFaceOffset > m_maxFaces * 0.7f) {
        float faceFragmentation = 1.0f - (static_cast<float>(totalActiveFaces) / m_currentFaceOffset);

        if (faceFragmentation > 0.3f) {
            needsCompaction = true;
            spdlog::debug("Buffer compaction needed: {:.1f}% face fragmentation",
                         faceFragmentation * 100);
        }
    }

    if (needsCompaction) {
        fullRebuild();
    }
}

void ChunkBufferManager::fullRebuild() {
    m_currentFaceOffset = 0;
    m_currentLightingOffset = 0;
    m_drawCommandCount = 0;
    m_chunkDataArray.clear();

    void* faceData = m_faceBuffer.map();
    void* lightingData = m_lightingBuffer.map();
    void* indirectData = m_indirectBuffer.map();
    void* chunkData = m_chunkDataBuffer.map();

    std::unordered_map<ChunkPosition, ChunkBufferAllocation, ChunkPositionHash> newAllocations;

    for (const auto& [pos, mesh] : m_meshCache) {
        if (mesh.faces.empty()) continue;

        // Write FaceData: copy then adjust lightIndex from local to global
        FaceData* destFaces = static_cast<FaceData*>(faceData) + m_currentFaceOffset;
        memcpy(destFaces, mesh.faces.data(), mesh.faces.size() * sizeof(FaceData));

        // Adjust lightIndex from local (chunk-relative) to global (buffer-relative)
        for (size_t j = 0; j < mesh.faces.size(); j++) {
            uint32_t localLightIndex = (destFaces[j].packed1 >> 16) & 0xFFFF;
            uint32_t globalLightIndex = m_currentLightingOffset + localLightIndex;
            destFaces[j].packed1 = (destFaces[j].packed1 & 0xFFFF) | (globalLightIndex << 16);
        }

        // Write lighting data
        memcpy(static_cast<uint8_t*>(lightingData) + m_currentLightingOffset * sizeof(PackedLighting),
               mesh.lighting.data(),
               mesh.lighting.size() * sizeof(PackedLighting));

        // Create and store ChunkData (indexed by gl_BaseInstance = drawCommandIndex)
        // faceOffset == lightingOffset since they're both incremented by the same amount
        ChunkData chunkMetadata = ChunkData::create(mesh.position, m_currentFaceOffset);
        m_chunkDataArray.push_back(chunkMetadata);
        memcpy(static_cast<uint8_t*>(chunkData) + m_drawCommandCount * sizeof(ChunkData),
               &chunkMetadata,
               sizeof(ChunkData));

        // Create draw command
        VkDrawIndirectCommand cmd{};
        cmd.vertexCount = 6;
        cmd.instanceCount = static_cast<uint32_t>(mesh.faces.size());
        cmd.firstVertex = 0;
        cmd.firstInstance = m_drawCommandCount;  // Chunk ID for gl_BaseInstance (indexes into ChunkData buffer)

        memcpy(static_cast<uint8_t*>(indirectData) + m_drawCommandCount * sizeof(VkDrawIndirectCommand),
               &cmd,
               sizeof(VkDrawIndirectCommand));

        // Store allocation
        ChunkBufferAllocation allocation;
        allocation.faceOffset = m_currentFaceOffset;
        allocation.faceCount = static_cast<uint32_t>(mesh.faces.size());
        allocation.lightingOffset = m_currentLightingOffset;
        allocation.drawCommandIndex = m_drawCommandCount;

        newAllocations[pos] = allocation;

        // Important: increment lighting offset by number of unique lighting values, not faces!
        uint32_t lightingCount = static_cast<uint32_t>(mesh.lighting.size());
        m_currentFaceOffset += allocation.faceCount;
        m_currentLightingOffset += lightingCount;  // Number of unique lighting values
        m_drawCommandCount++;
    }

    m_allocations = std::move(newAllocations);

    m_lightingBuffer.unmap();
    m_chunkDataBuffer.unmap();
    m_faceBuffer.unmap();
    m_indirectBuffer.unmap();

    spdlog::debug("Buffer compacted: {} chunks, {} faces",
                 m_meshCache.size(), m_currentFaceOffset);
}

bool ChunkBufferManager::hasAllocation(const ChunkPosition& pos) const {
    return m_allocations.find(pos) != m_allocations.end();
}

} // namespace VoxelEngine