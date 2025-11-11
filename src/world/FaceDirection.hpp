#pragma once

#include <cstdint>

namespace FarHorizon {

// Face direction enum for block faces
enum class FaceDirection : uint8_t {
    DOWN,
    UP,
    NORTH,
    SOUTH,
    WEST,
    EAST
};

// Get the opposite face direction (like Minecraft's Direction.getOpposite())
inline FaceDirection getOpposite(FaceDirection face) {
    switch (face) {
        case FaceDirection::DOWN:  return FaceDirection::UP;
        case FaceDirection::UP:    return FaceDirection::DOWN;
        case FaceDirection::NORTH: return FaceDirection::SOUTH;
        case FaceDirection::SOUTH: return FaceDirection::NORTH;
        case FaceDirection::WEST:  return FaceDirection::EAST;
        case FaceDirection::EAST:  return FaceDirection::WEST;
    }
    return face;  // Should never reach here
}

} // namespace FarHorizon