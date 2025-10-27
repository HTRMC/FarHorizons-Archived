#include "Chunk.hpp"
#include "BlockRegistry.hpp"
#include "blocks/SlabBlock.hpp"
#include <cstring>
#include <cmath>

namespace VoxelEngine {

Chunk::Chunk(const ChunkPosition& position)
    : m_position(position)
{
    // Initialize all blocks to AIR (palette index 0)
    std::memset(m_data.data(), 0, CHUNK_VOLUME * sizeof(uint8_t));
}

uint32_t Chunk::getBlockIndex(uint32_t x, uint32_t y, uint32_t z) const {
    return x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
}

// BlockState methods
BlockState Chunk::getBlockState(uint32_t x, uint32_t y, uint32_t z) const {
    uint32_t index = getBlockIndex(x, y, z);
    uint8_t paletteIndex = m_data[index];
    uint16_t stateId = m_palette.getStateId(paletteIndex);
    return BlockState(stateId);
}

void Chunk::setBlockState(uint32_t x, uint32_t y, uint32_t z, BlockState state) {
    uint32_t index = getBlockIndex(x, y, z);
    uint8_t paletteIndex = m_palette.getOrAddIndex(state.id);
    m_data[index] = paletteIndex;

    if (state.id != 0) {
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
                BlockState state = BlockRegistry::AIR->getDefaultState();

                if (worldPos.y >= 0.0f && worldPos.y <= 1.0f) {
                    // Place stone blocks at bottom layers
                    state = BlockRegistry::STONE->getDefaultState();
                } else {
                    glm::vec3 center(0.0f, 10.0f, 0.0f);
                    float distance = glm::length(worldPos - center);

                    if (distance >= 20.0f && distance <= 30.0f) {
                        // Place slabs in a sphere
                        // Use "top" variant for upper half of blocks (y % 2 == 1)
                        // Use "bottom" variant for lower half
                        SlabBlock* slabBlock = static_cast<SlabBlock*>(BlockRegistry::STONE_SLAB);
                        if (static_cast<int>(worldPos.y) % 2 == 1) {
                            state = slabBlock->withType(SlabType::TOP);
                        } else {
                            state = slabBlock->withType(SlabType::BOTTOM);
                        }
                    }
                }

                if (!state.isAir()) {
                    setBlockState(x, y, z, state);
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