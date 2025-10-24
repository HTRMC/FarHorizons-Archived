#include "Chunk.hpp"
#include <cstring>
#include <cmath>

namespace VoxelEngine {

Chunk::Chunk(const ChunkPosition& position)
    : m_position(position)
{
    std::memset(m_data.data(), 0, CHUNK_BIT_ARRAY_SIZE);
}

uint32_t Chunk::getVoxelIndex(uint32_t x, uint32_t y, uint32_t z) const {
    return x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
}

bool Chunk::getVoxel(uint32_t x, uint32_t y, uint32_t z) const {
    uint32_t index = getVoxelIndex(x, y, z);
    uint32_t byteIndex = index / 8;
    uint32_t bitIndex = index % 8;
    return (m_data[byteIndex] >> bitIndex) & 1;
}

void Chunk::setVoxel(uint32_t x, uint32_t y, uint32_t z, bool solid) {
    uint32_t index = getVoxelIndex(x, y, z);
    uint32_t byteIndex = index / 8;
    uint32_t bitIndex = index % 8;

    if (solid) {
        m_data[byteIndex] |= (1 << bitIndex);
        m_isEmpty = false;
    } else {
        m_data[byteIndex] &= ~(1 << bitIndex);
    }
}

void Chunk::generate() {
    glm::vec3 chunkWorldPos(
        m_position.x * static_cast<int32_t>(CHUNK_SIZE),
        m_position.y * static_cast<int32_t>(CHUNK_SIZE),
        m_position.z * static_cast<int32_t>(CHUNK_SIZE)
    );

    bool hasVoxels = false;

    for (uint32_t x = 0; x < CHUNK_SIZE; x++) {
        for (uint32_t y = 0; y < CHUNK_SIZE; y++) {
            for (uint32_t z = 0; z < CHUNK_SIZE; z++) {
                glm::vec3 worldPos = chunkWorldPos + glm::vec3(x, y, z);
                bool isSolid = false;

                if (worldPos.y >= 0.0f && worldPos.y <= 1.0f) {
                    isSolid = true;
                } else {
                    glm::vec3 center(0.0f, 10.0f, 0.0f);
                    float distance = glm::length(worldPos - center);

                    if (distance >= 20.0f && distance <= 30.0f) {
                        isSolid = true;
                    }
                }

                if (isSolid) {
                    setVoxel(x, y, z, true);
                    hasVoxels = true;
                }
            }
        }
    }

    if (hasVoxels) {
        m_isEmpty = false;
    }
}

} // namespace VoxelEngine