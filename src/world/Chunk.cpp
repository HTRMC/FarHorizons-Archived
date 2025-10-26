#include "Chunk.hpp"
#include <cstring>
#include <cmath>

namespace VoxelEngine {

Chunk::Chunk(const ChunkPosition& position)
    : m_position(position)
{
    // Initialize all blocks to AIR (palette index 0)
    std::memset(m_data.data(), 0, CHUNK_VOLUME * sizeof(uint16_t));
}

uint32_t Chunk::getBlockIndex(uint32_t x, uint32_t y, uint32_t z) const {
    return x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
}

// Legacy BlockType methods
BlockType Chunk::getBlock(uint32_t x, uint32_t y, uint32_t z) const {
    BlockStateRegistry::BlockStateId stateId = getBlockStateId(x, y, z);
    const BlockState& state = BlockStateRegistry::getInstance().getBlockState(stateId);
    return state.getType();
}

void Chunk::setBlock(uint32_t x, uint32_t y, uint32_t z, BlockType type) {
    setBlockState(x, y, z, type, "");
}

// New BlockState methods
BlockStateRegistry::BlockStateId Chunk::getBlockStateId(uint32_t x, uint32_t y, uint32_t z) const {
    uint32_t index = getBlockIndex(x, y, z);
    ChunkPalette::LocalIndex paletteIndex = m_data[index];
    return m_palette.getStateId(paletteIndex);
}

void Chunk::setBlockState(uint32_t x, uint32_t y, uint32_t z, BlockStateRegistry::BlockStateId stateId) {
    uint32_t index = getBlockIndex(x, y, z);
    ChunkPalette::LocalIndex paletteIndex = m_palette.getOrAddIndex(stateId);
    m_data[index] = paletteIndex;

    if (stateId != BlockStateRegistry::AIR_ID) {
        m_isEmpty = false;
    }
}

void Chunk::setBlockState(uint32_t x, uint32_t y, uint32_t z, BlockType type, const std::string& variant) {
    BlockStateRegistry::BlockStateId stateId =
        BlockStateRegistry::getInstance().getOrCreateId(type, variant);
    setBlockState(x, y, z, stateId);
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
                std::string variant = "";

                if (worldPos.y >= 0.0f && worldPos.y <= 1.0f) {
                    blockType = BlockType::STONE;
                } else {
                    glm::vec3 center(0.0f, 10.0f, 0.0f);
                    float distance = glm::length(worldPos - center);

                    if (distance >= 20.0f && distance <= 30.0f) {
                        blockType = BlockType::STONE_SLAB;

                        // Use "top" variant for upper half of blocks (y % 2 == 1)
                        // Use default variant for lower half
                        if (static_cast<int>(worldPos.y) % 2 == 1) {
                            variant = "top";
                        }
                    }
                }

                if (blockType != BlockType::AIR) {
                    setBlockState(x, y, z, blockType, variant);
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