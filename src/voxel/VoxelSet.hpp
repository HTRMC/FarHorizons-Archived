#pragma once

#include "util/Direction.hpp"
#include <stdexcept>

namespace FarHorizon {

// Base class for 3D voxel grids (from Minecraft's VoxelSet.java)
class VoxelSet {
protected:
    int sizeX;
    int sizeY;
    int sizeZ;

public:
    VoxelSet(int sizeX, int sizeY, int sizeZ)
        : sizeX(sizeX), sizeY(sizeY), sizeZ(sizeZ) {
        if (sizeX < 0 || sizeY < 0 || sizeZ < 0) {
            throw std::invalid_argument("Need all positive sizes");
        }
    }

    virtual ~VoxelSet() = default;

    // Core abstract methods
    virtual bool contains(int x, int y, int z) const = 0;
    virtual void set(int x, int y, int z) = 0;
    virtual int getMin(Direction::Axis axis) const = 0;
    virtual int getMax(Direction::Axis axis) const = 0;
    virtual bool isEmpty() const = 0;

    // Get size along an axis
    int getSize(Direction::Axis axis) const {
        return Direction::choose(axis, sizeX, sizeY, sizeZ);
    }

    int getXSize() const { return sizeX; }
    int getYSize() const { return sizeY; }
    int getZSize() const { return sizeZ; }

    // Check if position is in bounds and contains a voxel (VoxelSet.java line 62)
    bool inBoundsAndContains(int x, int y, int z) const {
        if (x < 0 || y < 0 || z < 0) {
            return false;
        }
        if (x >= sizeX || y >= sizeY || z >= sizeZ) {
            return false;
        }
        return contains(x, y, z);
    }

    // Version with axis cycling (VoxelSet.java line 58)
    bool inBoundsAndContains(AxisCycleDirection cycle, int x, int y, int z) const {
        int cx = AxisCycle::choose(cycle, x, y, z, Direction::Axis::X);
        int cy = AxisCycle::choose(cycle, x, y, z, Direction::Axis::Y);
        int cz = AxisCycle::choose(cycle, x, y, z, Direction::Axis::Z);
        return inBoundsAndContains(cx, cy, cz);
    }

    // Contains with axis cycling (VoxelSet.java line 70)
    bool contains(AxisCycleDirection cycle, int x, int y, int z) const {
        int cx = AxisCycle::choose(cycle, x, y, z, Direction::Axis::X);
        int cy = AxisCycle::choose(cycle, x, y, z, Direction::Axis::Y);
        int cz = AxisCycle::choose(cycle, x, y, z, Direction::Axis::Z);
        return contains(cx, cy, cz);
    }
};

} // namespace FarHorizon