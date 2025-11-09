#pragma once
#include <cstdint>

namespace VoxelEngine {

// Stair half property values (top or bottom of block)
enum class StairHalf : uint8_t {
    BOTTOM = 0,
    TOP = 1
};

} // namespace VoxelEngine