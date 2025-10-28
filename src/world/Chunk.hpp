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

    // Calculate Euclidean distance to another chunk position
    float distanceTo(const ChunkPosition& other) const {
        int32_t dx = x - other.x;
        int32_t dy = y - other.y;
        int32_t dz = z - other.z;
        return std::sqrt(static_cast<float>(dx*dx + dy*dy + dz*dz));
    }

    // Get the 6 face-adjacent neighbor positions (Minecraft's directDependencies)
    // Order: West, East, Down, Up, North, South
    static constexpr std::array<glm::ivec3, 6> getFaceNeighborOffsets() {
        return {{
            {-1,  0,  0},  // West
            { 1,  0,  0},  // East
            { 0, -1,  0},  // Down
            { 0,  1,  0},  // Up
            { 0,  0, -1},  // North
            { 0,  0,  1}   // South
        }};
    }

    // Get neighbor position in a specific direction
    ChunkPosition getNeighbor(int dx, int dy, int dz) const {
        return {x + dx, y + dy, z + dz};
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

    // Mesh dirty flag - tracks if chunk needs remeshing
    bool isDirty() const { return m_isDirty; }
    void markDirty() { m_isDirty = true; }
    void clearDirty() { m_isDirty = false; }

    // Access to palette
    const ChunkPalette& getPalette() const { return m_palette; }

private:
    ChunkPosition m_position;
    ChunkPalette m_palette;  // Maps local indices to global blockstate IDs
    std::array<uint8_t, CHUNK_VOLUME> m_data;  // Each uint8_t stores a palette index
    bool m_isEmpty = true;
    bool m_isDirty = false;  // Tracks if chunk needs remeshing

    uint32_t getBlockIndex(uint32_t x, uint32_t y, uint32_t z) const;
};

} // namespace VoxelEngine