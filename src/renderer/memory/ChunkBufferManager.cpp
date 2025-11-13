#include "ChunkBufferManager.hpp"
#include <spdlog/spdlog.h>

namespace FarHorizon {

void ChunkBufferManager::init(VmaAllocator allocator, size_t maxFaces, size_t maxDrawCommands) {
    maxFaces_ = maxFaces;
    maxDrawCommands_ = maxDrawCommands;

    // FaceData buffer (8 bytes per face, used as SSBO)
    faceBuffer_.init(
        allocator,
        maxFaces * sizeof(FaceData),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    // Lighting buffer (16 bytes per face)
    lightingBuffer_.init(
        allocator,
        maxFaces * sizeof(PackedLighting),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    // Indirect draw buffer (non-indexed instanced drawing)
    indirectBuffer_.init(
        allocator,
        maxDrawCommands * sizeof(VkDrawIndirectCommand),
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    // ChunkData buffer (per-chunk metadata, indexed by gl_BaseInstance)
    chunkDataBuffer.init(
        allocator,
        maxDrawCommands * sizeof(ChunkData),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    // Reserve space for chunk data array
    chunkDataArray_.reserve(maxDrawCommands);

    spdlog::info("ChunkBufferManager initialized: {} max faces, {} max draw commands", maxFaces, maxDrawCommands);
}

void ChunkBufferManager::cleanup() {
    chunkDataBuffer.cleanup();
    indirectBuffer_.cleanup();
    lightingBuffer_.cleanup();
    faceBuffer_.cleanup();
    meshCache_.clear();
    allocations_.clear();
    chunkDataArray_.clear();
}

void ChunkBufferManager::clear() {
    meshCache_.clear();
    allocations_.clear();
    chunkDataArray_.clear();
    currentFaceOffset_ = 0;
    currentLightingOffset_ = 0;
    drawCommandCount_ = 0;
    spdlog::info("Cleared all chunk meshes from GPU buffers");
}

bool ChunkBufferManager::addMeshes(std::vector<CompactChunkMesh>& meshes, size_t maxPerFrame) {
    if (meshes.empty()) return true;

    size_t processCount = std::min(meshes.size(), maxPerFrame);
    size_t actualProcessed = 0;
    bool needsDrawCommandRebuild = false;

    // Map buffers once for all updates
    void* faceData = faceBuffer_.map();
    void* lightingData = lightingBuffer_.map();
    void* indirectData = indirectBuffer_.map();
    void* chunkData = chunkDataBuffer.map();

    for (size_t i = 0; i < processCount; i++) {
        CompactChunkMesh& mesh = meshes[i];

        // Check if this chunk already has a mesh (update case)
        auto existingIt = allocations_.find(mesh.position);
        bool isUpdate = (existingIt != allocations_.end());

        // If updating and new mesh is empty, mark old allocation as unused and remove from cache
        if (isUpdate && mesh.faces.empty()) {
            allocations_.erase(existingIt);
            meshCache_.erase(mesh.position);
            actualProcessed++;
            needsDrawCommandRebuild = true;
            continue;
        }

        // Skip if mesh is empty and not updating (new empty chunk)
        if (mesh.faces.empty()) {
            actualProcessed++;
            continue;
        }

        // If updating, remove old allocation first (we'll add the new one below)
        if (isUpdate) {
            allocations_.erase(existingIt);
            meshCache_.erase(mesh.position);
            needsDrawCommandRebuild = true;
        }

        // Check if we have space
        if (currentFaceOffset_ + mesh.faces.size() > maxFaces_ ||
            drawCommandCount_ >= maxDrawCommands_) {
            spdlog::warn("Buffer full, cannot add more meshes");
            break;
        }

        // Write FaceData: copy then adjust lightIndex from local to global
        FaceData* destFaces = static_cast<FaceData*>(faceData) + currentFaceOffset_;
        memcpy(destFaces, mesh.faces.data(), mesh.faces.size() * sizeof(FaceData));

        // Adjust lightIndex from local (chunk-relative) to global (buffer-relative)
        for (size_t j = 0; j < mesh.faces.size(); j++) {
            uint32_t localLightIndex = (destFaces[j].packed1 >> 16) & 0xFFFF;
            uint32_t globalLightIndex = currentLightingOffset_ + localLightIndex;
            destFaces[j].packed1 = (destFaces[j].packed1 & 0xFFFF) | (globalLightIndex << 16);
        }

        // Write lighting data
        memcpy(static_cast<uint8_t*>(lightingData) + currentLightingOffset_ * sizeof(PackedLighting),
               mesh.lighting.data(),
               mesh.lighting.size() * sizeof(PackedLighting));

        // Create and store ChunkData (indexed by gl_BaseInstance = drawCommandIndex)
        // faceOffset == lightingOffset since they're both incremented by the same amount
        ChunkData chunkMetadata = ChunkData::create(mesh.position, currentFaceOffset_);
        chunkDataArray_.push_back(chunkMetadata);
        memcpy(static_cast<uint8_t*>(chunkData) + drawCommandCount_ * sizeof(ChunkData),
               &chunkMetadata,
               sizeof(ChunkData));

        // Create draw command (instanced non-indexed: 6 vertices per face)
        VkDrawIndirectCommand cmd{};
        cmd.vertexCount = 6;  // 2 triangles per quad (6 vertices total)
        cmd.instanceCount = static_cast<uint32_t>(mesh.faces.size());  // One instance per face
        cmd.firstVertex = 0;
        cmd.firstInstance = drawCommandCount_;  // Chunk ID for gl_BaseInstance (indexes into ChunkData buffer)

        memcpy(static_cast<uint8_t*>(indirectData) + drawCommandCount_ * sizeof(VkDrawIndirectCommand),
               &cmd,
               sizeof(VkDrawIndirectCommand));

        // Store allocation info
        ChunkBufferAllocation allocation;
        allocation.faceOffset = currentFaceOffset_;
        allocation.faceCount = static_cast<uint32_t>(mesh.faces.size());
        allocation.lightingOffset = currentLightingOffset_;
        allocation.drawCommandIndex = drawCommandCount_;

        allocations_[mesh.position] = allocation;

        // Important: increment lighting offset by number of unique lighting values, not faces!
        uint32_t lightingCount = static_cast<uint32_t>(mesh.lighting.size());
        currentFaceOffset_ += allocation.faceCount;
        currentLightingOffset_ += lightingCount;  // Number of unique lighting values
        drawCommandCount_++;
        actualProcessed++;

        meshCache_[mesh.position] = std::move(mesh);
    }

    lightingBuffer_.unmap();
    chunkDataBuffer.unmap();
    faceBuffer_.unmap();
    indirectBuffer_.unmap();

    // Rebuild draw commands if any chunks were removed/updated
    if (needsDrawCommandRebuild) {
        rebuildDrawCommands();
    }

    spdlog::trace("Added {} chunks to buffer ({} total)", actualProcessed, meshCache_.size());
    return true;
}

void ChunkBufferManager::removeUnloadedChunks(const ChunkManager& chunkManager) {
    std::vector<ChunkPosition> toRemove;

    for (const auto& [pos, allocation] : allocations_) {
        if (!chunkManager.hasChunk(pos)) {
            toRemove.push_back(pos);
        }
    }

    if (!toRemove.empty()) {
        for (const auto& pos : toRemove) {
            meshCache_.erase(pos);
            allocations_.erase(pos);
        }
        spdlog::debug("Removed {} unloaded chunks from buffer", toRemove.size());

        // Fast rebuild: only updates draw commands, leaves face/lighting data fragmented
        rebuildDrawCommands();
    }
}

void ChunkBufferManager::compactIfNeeded(const std::unordered_map<ChunkPosition, CompactChunkMesh, ChunkPositionHash>& meshCache) {
    // Calculate active space
    size_t totalActiveFaces = 0;
    for (const auto& [pos, allocation] : allocations_) {
        totalActiveFaces += allocation.faceCount;
    }

    // Check if compaction is needed
    bool needsCompaction = false;
    if (currentFaceOffset_ > maxFaces_ * 0.7f) {
        float faceFragmentation = 1.0f - (static_cast<float>(totalActiveFaces) / currentFaceOffset_);

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
    currentFaceOffset_ = 0;
    currentLightingOffset_ = 0;
    drawCommandCount_ = 0;
    chunkDataArray_.clear();

    void* faceData = faceBuffer_.map();
    void* lightingData = lightingBuffer_.map();
    void* indirectData = indirectBuffer_.map();
    void* chunkData = chunkDataBuffer.map();

    std::unordered_map<ChunkPosition, ChunkBufferAllocation, ChunkPositionHash> newAllocations;

    for (const auto& [pos, mesh] : meshCache_) {
        if (mesh.faces.empty()) continue;

        // Write FaceData: copy then adjust lightIndex from local to global
        FaceData* destFaces = static_cast<FaceData*>(faceData) + currentFaceOffset_;
        memcpy(destFaces, mesh.faces.data(), mesh.faces.size() * sizeof(FaceData));

        // Adjust lightIndex from local (chunk-relative) to global (buffer-relative)
        for (size_t j = 0; j < mesh.faces.size(); j++) {
            uint32_t localLightIndex = (destFaces[j].packed1 >> 16) & 0xFFFF;
            uint32_t globalLightIndex = currentLightingOffset_ + localLightIndex;
            destFaces[j].packed1 = (destFaces[j].packed1 & 0xFFFF) | (globalLightIndex << 16);
        }

        // Write lighting data
        memcpy(static_cast<uint8_t*>(lightingData) + currentLightingOffset_ * sizeof(PackedLighting),
               mesh.lighting.data(),
               mesh.lighting.size() * sizeof(PackedLighting));

        // Create and store ChunkData (indexed by gl_BaseInstance = drawCommandIndex)
        // faceOffset == lightingOffset since they're both incremented by the same amount
        ChunkData chunkMetadata = ChunkData::create(mesh.position, currentFaceOffset_);
        chunkDataArray_.push_back(chunkMetadata);
        memcpy(static_cast<uint8_t*>(chunkData) + drawCommandCount_ * sizeof(ChunkData),
               &chunkMetadata,
               sizeof(ChunkData));

        // Create draw command
        VkDrawIndirectCommand cmd{};
        cmd.vertexCount = 6;
        cmd.instanceCount = static_cast<uint32_t>(mesh.faces.size());
        cmd.firstVertex = 0;
        cmd.firstInstance = drawCommandCount_;  // Chunk ID for gl_BaseInstance (indexes into ChunkData buffer)

        memcpy(static_cast<uint8_t*>(indirectData) + drawCommandCount_ * sizeof(VkDrawIndirectCommand),
               &cmd,
               sizeof(VkDrawIndirectCommand));

        // Store allocation
        ChunkBufferAllocation allocation;
        allocation.faceOffset = currentFaceOffset_;
        allocation.faceCount = static_cast<uint32_t>(mesh.faces.size());
        allocation.lightingOffset = currentLightingOffset_;
        allocation.drawCommandIndex = drawCommandCount_;

        newAllocations[pos] = allocation;

        // Important: increment lighting offset by number of unique lighting values, not faces!
        uint32_t lightingCount = static_cast<uint32_t>(mesh.lighting.size());
        currentFaceOffset_ += allocation.faceCount;
        currentLightingOffset_ += lightingCount;  // Number of unique lighting values
        drawCommandCount_++;
    }

    allocations_ = std::move(newAllocations);

    lightingBuffer_.unmap();
    chunkDataBuffer.unmap();
    faceBuffer_.unmap();
    indirectBuffer_.unmap();

    spdlog::debug("Buffer compacted: {} chunks, {} faces",
                 meshCache_.size(), currentFaceOffset_);
}

void ChunkBufferManager::rebuildDrawCommands() {
    // Fast path: only rebuild draw commands and chunk metadata
    // Face data and lighting data remain in place (potentially fragmented)
    // This is MUCH faster than fullRebuild() when unloading chunks

    drawCommandCount_ = 0;
    chunkDataArray_.clear();

    void* indirectData = indirectBuffer_.map();
    void* chunkData = chunkDataBuffer.map();

    // Rebuild draw commands for remaining allocations
    for (auto& [pos, allocation] : allocations_) {
        // Create draw command pointing to existing face/lighting data
        VkDrawIndirectCommand cmd{};
        cmd.vertexCount = 6;
        cmd.instanceCount = allocation.faceCount;
        cmd.firstVertex = 0;
        cmd.firstInstance = drawCommandCount_;  // New sequential index

        memcpy(static_cast<uint8_t*>(indirectData) + drawCommandCount_ * sizeof(VkDrawIndirectCommand),
               &cmd,
               sizeof(VkDrawIndirectCommand));

        // Create chunk metadata pointing to existing face data
        ChunkData chunkMetadata = ChunkData::create(pos, allocation.faceOffset);
        chunkDataArray_.push_back(chunkMetadata);
        memcpy(static_cast<uint8_t*>(chunkData) + drawCommandCount_ * sizeof(ChunkData),
               &chunkMetadata,
               sizeof(ChunkData));

        // Update allocation with new draw command index
        allocation.drawCommandIndex = drawCommandCount_;
        drawCommandCount_++;
    }

    indirectBuffer_.unmap();
    chunkDataBuffer.unmap();

    // spdlog::debug("Rebuilt draw commands: {} chunks (face/lighting data unchanged)",
    //              m_drawCommandCount);
}

bool ChunkBufferManager::hasAllocation(const ChunkPosition& pos) const {
    return allocations_.find(pos) != allocations_.end();
}

} // namespace FarHorizon