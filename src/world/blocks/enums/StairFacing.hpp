#pragma once
#include <cstdint>

namespace VoxelEngine {

// Stair facing property values (direction the stairs face)
enum class StairFacing : uint8_t {
    NORTH = 0,
    SOUTH = 1,
    WEST = 2,
    EAST = 3
};

} // namespace VoxelEngine