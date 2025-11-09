#pragma once
#include <cstdint>

namespace VoxelEngine {

// Block half property values (top or bottom of block)
enum class BlockHalf : uint8_t {
    BOTTOM = 0,
    TOP = 1
};

} // namespace VoxelEngine