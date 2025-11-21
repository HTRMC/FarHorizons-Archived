#pragma once
#include <vector>
#include <glm/glm.hpp>

namespace FarHorizon {

// Forward declare StairFacing from blocks/enums
enum class StairFacing : uint8_t;

// Direction and Axis utilities (from Minecraft's Direction.java)
class Direction {
public:
    enum class Axis {
        X,
        Y,
        Z
    };

    // Check if axis is horizontal (X or Z, not Y)
    // From Minecraft's Direction.Axis.isHorizontal()
    static bool isHorizontal(Axis axis) {
        return axis == Axis::X || axis == Axis::Z;
    }

    // Get axis from a direction vector
    static Axis getAxis(const glm::ivec3& direction) {
        if (direction.x != 0) return Axis::X;
        if (direction.y != 0) return Axis::Y;
        if (direction.z != 0) return Axis::Z;
        return Axis::Y;  // Default for zero vector
    }

    // Utility to choose a value based on axis
    template<typename T>
    static T choose(Axis axis, T x, T y, T z) {
        switch (axis) {
            case Axis::X: return x;
            case Axis::Y: return y;
            case Axis::Z: return z;
            default: return x;
        }
    }

    // Axis step order for collision resolution (Direction.java line 539)
    // Returns the order in which to resolve collision axes based on movement direction
    // Minecraft: public static ImmutableList<Axis> axisStepOrder(final Vec3 movement)
    template<typename Vec3>
    static std::vector<Axis> axisStepOrder(const Vec3& movement) {
        // Direction.java line 540: return Math.abs(movement.x) < Math.abs(movement.z) ? YZX_AXIS_ORDER : YXZ_AXIS_ORDER;
        // YZX_AXIS_ORDER = [Y, Z, X] - use when Z movement is larger
        // YXZ_AXIS_ORDER = [Y, X, Z] - use when X movement is larger or equal
        if (std::abs(movement.x) < std::abs(movement.z)) {
            return {Axis::Y, Axis::Z, Axis::X};  // YZX
        } else {
            return {Axis::Y, Axis::X, Axis::Z};  // YXZ
        }
    }
};

// Horizontal Direction utilities (from Minecraft's Direction.java)
// Works with horizontal-only direction enums like StairFacing
// Assumes enum values: NORTH=0, SOUTH=1, WEST=2, EAST=3
class HorizontalDirection {
public:
    // Get opposite direction (Direction.java getOpposite() for horizontal)
    // NORTH <-> SOUTH, EAST <-> WEST
    template<typename T>
    static T getOpposite(T facing) {
        // Mapping: NORTH(0)->SOUTH(1), SOUTH(1)->NORTH(0), WEST(2)->EAST(3), EAST(3)->WEST(2)
        static const int opposites[] = {1, 0, 3, 2};  // [NORTH, SOUTH, WEST, EAST]
        return static_cast<T>(opposites[static_cast<int>(facing)]);
    }

    // Rotate counter-clockwise (Direction.java getCounterClockWise() for horizontal)
    // NORTH -> WEST -> SOUTH -> EAST -> NORTH
    template<typename T>
    static T getCounterClockWise(T facing) {
        // NORTH(0)->WEST(2), SOUTH(1)->EAST(3), WEST(2)->SOUTH(1), EAST(3)->NORTH(0)
        static const int ccw[] = {2, 3, 1, 0};  // [NORTH, SOUTH, WEST, EAST]
        return static_cast<T>(ccw[static_cast<int>(facing)]);
    }

    // Rotate clockwise (Direction.java getClockWise() for horizontal)
    // NORTH -> EAST -> SOUTH -> WEST -> NORTH
    template<typename T>
    static T getClockWise(T facing) {
        // NORTH(0)->EAST(3), SOUTH(1)->WEST(2), WEST(2)->NORTH(0), EAST(3)->SOUTH(1)
        static const int cw[] = {3, 2, 0, 1};  // [NORTH, SOUTH, WEST, EAST]
        return static_cast<T>(cw[static_cast<int>(facing)]);
    }

    // Get axis for horizontal direction (Direction.java getAxis())
    // NORTH/SOUTH -> Z axis, EAST/WEST -> X axis
    template<typename T>
    static Direction::Axis getAxis(T facing) {
        int f = static_cast<int>(facing);
        // NORTH(0)->Z, SOUTH(1)->Z, WEST(2)->X, EAST(3)->X
        return (f == 0 || f == 1) ? Direction::Axis::Z : Direction::Axis::X;
    }

    // Get offset vector for a facing direction (for pos.relative(direction) in Java)
    template<typename T>
    static glm::ivec3 getOffset(T facing) {
        switch (static_cast<int>(facing)) {
            case 0: return glm::ivec3(0, 0, -1);  // NORTH
            case 1: return glm::ivec3(0, 0, 1);   // SOUTH
            case 2: return glm::ivec3(-1, 0, 0);  // WEST
            case 3: return glm::ivec3(1, 0, 0);   // EAST
            default: return glm::ivec3(0, 0, 0);
        }
    }
};

// Axis cycle direction (from Minecraft's AxisCycleDirection.java)
enum class AxisCycleDirection {
    NONE,
    FORWARD,
    BACKWARD
};

namespace AxisCycle {
    // Cycle axis forward: X -> Y -> Z -> X
    inline Direction::Axis cycle(Direction::Axis axis) {
        switch (axis) {
            case Direction::Axis::X: return Direction::Axis::Y;
            case Direction::Axis::Y: return Direction::Axis::Z;
            case Direction::Axis::Z: return Direction::Axis::X;
            default: return Direction::Axis::X;
        }
    }

    // Cycle axis backward: X -> Z -> Y -> X
    inline Direction::Axis cycleBackward(Direction::Axis axis) {
        switch (axis) {
            case Direction::Axis::X: return Direction::Axis::Z;
            case Direction::Axis::Y: return Direction::Axis::X;
            case Direction::Axis::Z: return Direction::Axis::Y;
            default: return Direction::Axis::X;
        }
    }

    // Apply axis cycle direction
    inline Direction::Axis apply(AxisCycleDirection direction, Direction::Axis axis) {
        switch (direction) {
            case AxisCycleDirection::NONE: return axis;
            case AxisCycleDirection::FORWARD: return cycle(axis);
            case AxisCycleDirection::BACKWARD: return cycleBackward(axis);
            default: return axis;
        }
    }

    // Get opposite direction
    inline AxisCycleDirection opposite(AxisCycleDirection direction) {
        switch (direction) {
            case AxisCycleDirection::NONE: return AxisCycleDirection::NONE;
            case AxisCycleDirection::FORWARD: return AxisCycleDirection::BACKWARD;
            case AxisCycleDirection::BACKWARD: return AxisCycleDirection::FORWARD;
            default: return AxisCycleDirection::NONE;
        }
    }

    // Between two axes (get the cycle direction needed to go from 'from' to 'to')
    inline AxisCycleDirection between(Direction::Axis from, Direction::Axis to) {
        if (from == to) return AxisCycleDirection::NONE;
        if ((from == Direction::Axis::X && to == Direction::Axis::Y) ||
            (from == Direction::Axis::Y && to == Direction::Axis::Z) ||
            (from == Direction::Axis::Z && to == Direction::Axis::X)) {
            return AxisCycleDirection::FORWARD;
        }
        return AxisCycleDirection::BACKWARD;
    }

    // Choose value based on cycled axis (AxisCycleDirection.choose)
    template<typename T>
    inline T choose(AxisCycleDirection direction, T x, T y, T z, Direction::Axis axis) {
        Direction::Axis cycled = apply(direction, axis);
        return Direction::choose(cycled, x, y, z);
    }
}

} // namespace FarHorizon