#pragma once

#include "BlockState.hpp"
#include "ChunkPalette.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <array>

namespace VoxelEngine {

constexpr uint32_t CHUNK_SIZE = 16;
constexpr uint32_t CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

struct ChunkPosition {
    int32_t x, y, z;

    bool operator==(const ChunkPosition& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const ChunkPosition& other) const {
        return !(*this == other);
    }
};

struct ChunkPositionHash {
    std::size_t operator()(const ChunkPosition& pos) const {
        std::size_t h1 = std::hash<int32_t>{}(pos.x);
        std::size_t h2 = std::hash<int32_t>{}(pos.y);
        std::size_t h3 = std::hash<int32_t>{}(pos.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

class Chunk {
public:
    Chunk(const ChunkPosition& position);

    const ChunkPosition& getPosition() const { return m_position; }

    // BlockState methods
    BlockState getBlockState(uint32_t x, uint32_t y, uint32_t z) const;
    void setBlockState(uint32_t x, uint32_t y, uint32_t z, BlockState state);

    const uint8_t* getData() const { return m_data.data(); }

    void generate();

    bool isEmpty() const { return m_palette.isEmpty(); }
    void markEmpty() { m_isEmpty = true; }
    void markNonEmpty() { m_isEmpty = false; }

    // Access to palette
    const ChunkPalette& getPalette() const { return m_palette; }

private:
    ChunkPosition m_position;
    ChunkPalette m_palette;  // Maps local indices to global blockstate IDs
    std::array<uint8_t, CHUNK_VOLUME> m_data;  // Each uint8_t stores a palette index
    bool m_isEmpty = true;

    uint32_t getBlockIndex(uint32_t x, uint32_t y, uint32_t z) const;
};

} // namespace VoxelEngine