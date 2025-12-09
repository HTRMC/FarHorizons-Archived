#include "ChunkData.hpp"
#include "BlockRegistry.hpp"
#include "blocks/SlabBlock.hpp"
#include <FastNoise/FastNoise.h>
#include <tracy/Tracy.hpp>

namespace FarHorizon {

// Static noise generator for terrain
static FastNoise::SmartNode<> getTerrainNoise() {
    static auto noise = FastNoise::New<FastNoise::OpenSimplex2>();
    return noise;
}

ChunkData::ChunkData(const ChunkPosition& position)
    : position_(position)
    , palette_()
    , data_{}
    , empty_(true)
    , version_(0) {
}

ChunkData::ChunkData(const ChunkPosition& position,
                     ChunkPalette palette,
                     std::array<uint8_t, CHUNK_VOLUME> data,
                     bool empty,
                     uint32_t version)
    : position_(position)
    , palette_(std::move(palette))
    , data_(std::move(data))
    , empty_(empty)
    , version_(version) {
}

BlockState ChunkData::getBlockState(uint32_t x, uint32_t y, uint32_t z) const {
    uint32_t index = getBlockIndex(x, y, z);
    uint8_t paletteIndex = data_[index];
    return BlockState(palette_.getStateId(paletteIndex));
}

std::shared_ptr<const ChunkData> ChunkData::withBlockState(
    uint32_t x, uint32_t y, uint32_t z, BlockState state) const {
    ZoneScoped;

    // Copy palette and data
    ChunkPalette newPalette = palette_;
    std::array<uint8_t, CHUNK_VOLUME> newData = data_;

    // Add new state to palette and update data
    uint8_t paletteIndex = newPalette.getOrAddIndex(state.id);
    uint32_t blockIndex = getBlockIndex(x, y, z);
    newData[blockIndex] = paletteIndex;

    // Determine if chunk is now empty
    bool newEmpty = true;
    BlockState airState = BlockRegistry::AIR->getDefaultState();
    for (uint32_t i = 0; i < CHUNK_VOLUME; i++) {
        if (newPalette.getStateId(newData[i]) != airState.id) {
            newEmpty = false;
            break;
        }
    }

    return std::make_shared<const ChunkData>(
        position_,
        std::move(newPalette),
        std::move(newData),
        newEmpty,
        version_ + 1  // Increment version for mesh invalidation
    );
}

std::shared_ptr<const ChunkData> ChunkData::generate(const ChunkPosition& position) {
    ZoneScoped;

    static auto noise = getTerrainNoise();

    glm::vec3 chunkWorldPos(
        position.x * static_cast<int32_t>(CHUNK_SIZE),
        position.y * static_cast<int32_t>(CHUNK_SIZE),
        position.z * static_cast<int32_t>(CHUNK_SIZE)
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

    ChunkPalette palette;
    std::array<uint8_t, CHUNK_VOLUME> data{};
    bool hasBlocks = false;

    // Get palette indices for common blocks
    uint8_t airIndex = palette.getOrAddIndex(BlockRegistry::AIR->getDefaultState().id);
    uint8_t stoneIndex = palette.getOrAddIndex(BlockRegistry::STONE->getDefaultState().id);
    uint8_t grassIndex = palette.getOrAddIndex(BlockRegistry::GRASS_BLOCK->getDefaultState().id);

    // Fill with air initially
    data.fill(airIndex);

    for (uint32_t x = 0; x < CHUNK_SIZE; x++) {
        for (uint32_t z = 0; z < CHUNK_SIZE; z++) {
            int terrainHeight = terrainHeights[x][z];

            for (uint32_t y = 0; y < CHUNK_SIZE; y++) {
                glm::vec3 worldPos = chunkWorldPos + glm::vec3(x, y, z);
                uint32_t blockIndex = getBlockIndex(x, y, z);

                // OpenSimplex2 terrain generation
                if (worldPos.y <= terrainHeight) {
                    if (worldPos.y == terrainHeight) {
                        data[blockIndex] = grassIndex;
                    } else {
                        data[blockIndex] = stoneIndex;
                    }
                    hasBlocks = true;
                }

                // Stone slab sphere
                glm::vec3 center(0.0f, 50.0f, 0.0f);
                float distance = glm::length(worldPos - center);

                if (distance >= 20.0f && distance <= 30.0f) {
                    SlabBlock* slabBlock = static_cast<SlabBlock*>(BlockRegistry::STONE_SLAB);
                    BlockState slabState;
                    if (static_cast<int>(worldPos.y) % 2 == 1) {
                        slabState = slabBlock->withType(SlabType::TOP);
                    } else {
                        slabState = slabBlock->withType(SlabType::BOTTOM);
                    }
                    uint8_t slabIndex = palette.getOrAddIndex(slabState.id);
                    data[blockIndex] = slabIndex;
                    hasBlocks = true;
                }
            }
        }
    }

    return std::make_shared<const ChunkData>(
        position,
        std::move(palette),
        std::move(data),
        !hasBlocks,
        0  // Initial version
    );
}

} // namespace FarHorizon