#pragma once
#include <cstdint>

namespace VoxelEngine {

// Slab type property values
enum class SlabType : uint8_t {
    BOTTOM = 0,
    TOP = 1,
    DOUBLE = 2
};

} // namespace VoxelEngine