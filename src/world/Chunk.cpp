#include "Chunk.hpp"
#include "BlockRegistry.hpp"
#include "blocks/SlabBlock.hpp"
#include <FastNoise/FastNoise.h>
#include <cstring>
#include <cmath>

namespace FarHorizon {

// Static terrain noise generator using OpenSimplex2
static FastNoise::SmartNode<> getTerrainNoise() {
    static auto noise = FastNoise::New<FastNoise::OpenSimplex2>();
    return noise;
}

Chunk::Chunk(const ChunkPosition& position)
    : position_(position)
{
    // Initialize all blocks to AIR (palette index 0)
    std::memset(data_.data(), 0, CHUNK_VOLUME * sizeof(uint8_t));
}

uint32_t Chunk::getBlockIndex(uint32_t x, uint32_t y, uint32_t z) const {
    return x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
}

// BlockState methods
BlockState Chunk::getBlockState(uint32_t x, uint32_t y, uint32_t z) const {
    uint32_t index = getBlockIndex(x, y, z);
    uint8_t paletteIndex = data_[index];
    uint16_t stateId = palette_.getStateId(paletteIndex);
    return BlockState(stateId);
}

void Chunk::setBlockState(uint32_t x, uint32_t y, uint32_t z, BlockState state) {
    uint32_t index = getBlockIndex(x, y, z);
    uint8_t paletteIndex = palette_.getOrAddIndex(state.id);
    data_[index] = paletteIndex;

    if (state.id != 0) {
        isEmpty_ = false;
    }
}

void Chunk::generate() {
    static auto noise = getTerrainNoise();

    glm::vec3 chunkWorldPos(
        position_.x * static_cast<int32_t>(CHUNK_SIZE),
        position_.y * static_cast<int32_t>(CHUNK_SIZE),
        position_.z * static_cast<int32_t>(CHUNK_SIZE)
    );

    // Batch generate noise for the entire chunk's X/Z plane (SIMD optimized)
    std::vector<float> noiseOutput(CHUNK_SIZE * CHUNK_SIZE);
    constexpr float frequency = 0.02f;
    noise->GenUniformGrid2D(
        noiseOutput.data(),
        static_cast<int>(chunkWorldPos.x),
        static_cast<int>(chunkWorldPos.z),
        CHUNK_SIZE, CHUNK_SIZE,
        frequency, 1337
    );

    // Pre-compute terrain heights from noise
    int terrainHeights[CHUNK_SIZE][CHUNK_SIZE];
    for (uint32_t x = 0; x < CHUNK_SIZE; x++) {
        for (uint32_t z = 0; z < CHUNK_SIZE; z++) {
            float noiseValue = noiseOutput[x + z * CHUNK_SIZE];
            terrainHeights[x][z] = static_cast<int>((noiseValue + 1.0f) * 32.0f);
        }
    }

    bool hasBlocks = false;

    for (uint32_t x = 0; x < CHUNK_SIZE; x++) {
        for (uint32_t z = 0; z < CHUNK_SIZE; z++) {
            int terrainHeight = terrainHeights[x][z];

            for (uint32_t y = 0; y < CHUNK_SIZE; y++) {
                glm::vec3 worldPos = chunkWorldPos + glm::vec3(x, y, z);
                BlockState state = BlockRegistry::AIR->getDefaultState();

                // OpenSimplex2 terrain generation
                if (worldPos.y <= terrainHeight) {
                    if (worldPos.y == terrainHeight) {
                        // Surface layer: grass
                        state = BlockRegistry::GRASS_BLOCK->getDefaultState();
                    } else {
                        // Underground: stone
                        state = BlockRegistry::STONE->getDefaultState();
                    }
                }

                // Stone slab sphere (kept from original)
                glm::vec3 center(0.0f, 50.0f, 0.0f);
                float distance = glm::length(worldPos - center);

                if (distance >= 20.0f && distance <= 30.0f) {
                    SlabBlock* slabBlock = static_cast<SlabBlock*>(BlockRegistry::STONE_SLAB);
                    if (static_cast<int>(worldPos.y) % 2 == 1) {
                        state = slabBlock->withType(SlabType::TOP);
                    } else {
                        state = slabBlock->withType(SlabType::BOTTOM);
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
        isEmpty_ = false;
    }
}

} // namespace FarHorizon