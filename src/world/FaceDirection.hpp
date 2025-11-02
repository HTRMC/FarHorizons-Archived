#pragma once

#include <cstdint>

namespace VoxelEngine {

// Face direction enum for block faces
enum class FaceDirection : uint8_t {
    DOWN,
    UP,
    NORTH,
    SOUTH,
    WEST,
    EAST
};

} // namespace VoxelEngine