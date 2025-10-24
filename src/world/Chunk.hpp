#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <array>

namespace VoxelEngine {

constexpr uint32_t CHUNK_SIZE = 16;
constexpr uint32_t CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
constexpr uint32_t CHUNK_BIT_ARRAY_SIZE = CHUNK_VOLUME / 8;

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

    bool getVoxel(uint32_t x, uint32_t y, uint32_t z) const;
    void setVoxel(uint32_t x, uint32_t y, uint32_t z, bool solid);

    const uint8_t* getData() const { return m_data.data(); }

    void generate();

    bool isEmpty() const { return m_isEmpty; }
    void markEmpty() { m_isEmpty = true; }
    void markNonEmpty() { m_isEmpty = false; }

private:
    ChunkPosition m_position;
    std::array<uint8_t, CHUNK_BIT_ARRAY_SIZE> m_data;
    bool m_isEmpty = true;

    uint32_t getVoxelIndex(uint32_t x, uint32_t y, uint32_t z) const;
};

} // namespace VoxelEngine