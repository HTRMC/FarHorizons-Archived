#pragma once
#include <cstdint>

namespace VoxelEngine {

// Face directions for per-face queries
enum class Face : uint8_t {
    DOWN = 0,   // -Y
    UP = 1,     // +Y
    NORTH = 2,  // -Z
    SOUTH = 3,  // +Z
    WEST = 4,   // -X
    EAST = 5    // +X
};

// BlockStateNew is just a lightweight ID wrapper
// No logic, no virtual functions - just a uint16_t with helper methods
struct BlockStateNew {
    uint16_t id;

    BlockStateNew() : id(0) {}
    explicit BlockStateNew(uint16_t stateId) : id(stateId) {}

    bool operator==(const BlockStateNew& other) const { return id == other.id; }
    bool operator!=(const BlockStateNew& other) const { return id != other.id; }

    bool isAir() const { return id == 0; }
};

} // namespace VoxelEngine