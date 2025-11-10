#pragma once
#include <cstdint>

namespace FarHorizon {

// Stair shape property values (corner detection)
enum class StairShape : uint8_t {
    STRAIGHT = 0,
    INNER_LEFT = 1,
    INNER_RIGHT = 2,
    OUTER_LEFT = 3,
    OUTER_RIGHT = 4
};

} // namespace FarHorizon