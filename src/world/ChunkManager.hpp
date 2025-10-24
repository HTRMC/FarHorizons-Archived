#pragma once

#include "Chunk.hpp"
#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>
#include <vector>

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
};

class ChunkManager {
public:
    ChunkManager();

    void setRenderDistance(int32_t distance) { m_renderDistance = distance; }
    int32_t getRenderDistance() const { return m_renderDistance; }

    void update(const glm::vec3& cameraPosition);

    ChunkPosition worldToChunkPos(const glm::vec3& worldPos) const;

    Chunk* getChunk(const ChunkPosition& pos);
    const Chunk* getChunk(const ChunkPosition& pos) const;

    const std::unordered_map<ChunkPosition, std::unique_ptr<Chunk>, ChunkPositionHash>& getChunks() const {
        return m_chunks;
    }

    ChunkMesh generateChunkMesh(const Chunk* chunk, uint32_t textureIndex) const;

    bool hasChunksChanged() const { return m_chunksChanged; }
    void clearChangedFlag() { m_chunksChanged = false; }

private:
    std::unordered_map<ChunkPosition, std::unique_ptr<Chunk>, ChunkPositionHash> m_chunks;
    int32_t m_renderDistance = 8;
    ChunkPosition m_lastCameraChunkPos = {INT32_MAX, INT32_MAX, INT32_MAX};
    bool m_chunksChanged = false;

    void loadChunksAroundPosition(const ChunkPosition& centerPos);
    Chunk* loadChunk(const ChunkPosition& pos);
};

} // namespace VoxelEngine