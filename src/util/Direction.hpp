#pragma once
#include <vector>

namespace FarHorizon {

// Direction and Axis utilities (from Minecraft's Direction.java)
class Direction {
public:
    enum class Axis {
        X,
        Y,
        Z
    };

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