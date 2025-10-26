#include "Chunk.hpp"
#include <cstring>
#include <cmath>

namespace VoxelEngine {

Chunk::Chunk(const ChunkPosition& position)
    : m_position(position)
{
    // Initialize all blocks to AIR
    std::memset(m_data.data(), static_cast<uint8_t>(BlockType::AIR), CHUNK_VOLUME);
}

uint32_t Chunk::getBlockIndex(uint32_t x, uint32_t y, uint32_t z) const {
    return x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
}

BlockType Chunk::getBlock(uint32_t x, uint32_t y, uint32_t z) const {
    uint32_t index = getBlockIndex(x, y, z);
    return static_cast<BlockType>(m_data[index]);
}

void Chunk::setBlock(uint32_t x, uint32_t y, uint32_t z, BlockType type) {
    uint32_t index = getBlockIndex(x, y, z);
    m_data[index] = static_cast<uint8_t>(type);

    if (type != BlockType::AIR) {
        m_isEmpty = false;
    }
}

void Chunk::generate() {
    glm::vec3 chunkWorldPos(
        m_position.x * static_cast<int32_t>(CHUNK_SIZE),
        m_position.y * static_cast<int32_t>(CHUNK_SIZE),
        m_position.z * static_cast<int32_t>(CHUNK_SIZE)
    );

    bool hasBlocks = false;

    for (uint32_t x = 0; x < CHUNK_SIZE; x++) {
        for (uint32_t y = 0; y < CHUNK_SIZE; y++) {
            for (uint32_t z = 0; z < CHUNK_SIZE; z++) {
                glm::vec3 worldPos = chunkWorldPos + glm::vec3(x, y, z);
                BlockType blockType = BlockType::AIR;

                if (worldPos.y >= 0.0f && worldPos.y <= 1.0f) {
                    blockType = BlockType::STONE;
                } else {
                    glm::vec3 center(0.0f, 10.0f, 0.0f);
                    float distance = glm::length(worldPos - center);

                    if (distance >= 20.0f && distance <= 30.0f) {
                        blockType = BlockType::STONE_SLAB;
                    }
                }

                if (blockType != BlockType::AIR) {
                    setBlock(x, y, z, blockType);
                    hasBlocks = true;
                }
            }
        }
    }

    if (hasBlocks) {
        m_isEmpty = false;
    }
}

} // namespace VoxelEngine