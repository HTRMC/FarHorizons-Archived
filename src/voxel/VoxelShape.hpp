#pragma once

#include "VoxelSet.hpp"
#include "physics/AABB.hpp"
#include <vector>
#include <memory>

namespace FarHorizon {

// Abstract voxel shape (from Minecraft's VoxelShape.java)
class VoxelShape {
protected:
    std::shared_ptr<VoxelSet> voxels;

public:
    VoxelShape(std::shared_ptr<VoxelSet> voxels)
        : voxels(voxels) {}

    virtual ~VoxelShape() = default;

    // Get point positions along an axis (abstract)
    virtual const std::vector<double>& getPointPositions(Direction::Axis axis) const = 0;

    // Check if empty
    bool isEmpty() const {
        return voxels->isEmpty();
    }

    // Get voxel set
    const VoxelSet& getVoxels() const {
        return *voxels;
    }

    // Get position of a point index along an axis (VoxelShape.java line 67)
    double getPointPosition(Direction::Axis axis, int index) const {
        return getPointPositions(axis)[index];
    }

    // Get coordinate index for a position (VoxelShape.java line 272)
    int getCoordIndex(Direction::Axis axis, double pos) const {
        const auto& points = getPointPositions(axis);
        // Binary search for the position
        int size = static_cast<int>(points.size()) - 1;
        for (int i = 0; i < size; i++) {
            if (pos < points[i + 1] - 1.0E-7) {
                return i;
            }
        }
        return size;
    }

    // Calculate maximum distance before collision (VoxelShape.java line 256)
    // This is the CORE collision method!
    double calculateMaxDistance(Direction::Axis axis, const AABB& box, double maxDist) const {
        return calculateMaxDistance(AxisCycle::between(Direction::Axis::X, axis), box, maxDist);
    }

protected:
    // Internal calculateMaxDistance with axis cycling (VoxelShape.java line 260)
    double calculateMaxDistance(AxisCycleDirection axisCycle, const AABB& box, double maxDist) const {
        if (isEmpty()) {
            return maxDist;
        }
        if (std::abs(maxDist) < 1.0E-7) {
            return 0.0;
        }

        AxisCycleDirection axisCycleOpposite = AxisCycle::opposite(axisCycle);
        Direction::Axis axis = AxisCycle::apply(axisCycleOpposite, Direction::Axis::X);
        Direction::Axis axis2 = AxisCycle::apply(axisCycleOpposite, Direction::Axis::Y);
        Direction::Axis axis3 = AxisCycle::apply(axisCycleOpposite, Direction::Axis::Z);

        // Get box bounds along the cycled axes (VoxelShape.java line 270-271)
        double boxMax = Direction::choose(axis, box.maxX, box.maxY, box.maxZ);
        double boxMin = Direction::choose(axis, box.minX, box.minY, box.minZ);

        // Get voxel indices (VoxelShape.java line 272-273)
        int startIndex = getCoordIndex(axis, boxMin + 1.0E-7);
        int endIndex = getCoordIndex(axis, boxMax - 1.0E-7);

        // Get bounds for other axes (VoxelShape.java line 274-277)
        int k = std::max(0, getCoordIndex(axis2, Direction::choose(axis2, box.minX, box.minY, box.minZ) + 1.0E-7));
        int l = std::min(voxels->getSize(axis2), getCoordIndex(axis2, Direction::choose(axis2, box.maxX, box.maxY, box.maxZ) - 1.0E-7) + 1);
        int m = std::max(0, getCoordIndex(axis3, Direction::choose(axis3, box.minX, box.minY, box.minZ) + 1.0E-7));
        int n = std::min(voxels->getSize(axis3), getCoordIndex(axis3, Direction::choose(axis3, box.maxX, box.maxY, box.maxZ) - 1.0E-7) + 1);
        int o = voxels->getSize(axis);

        if (maxDist > 0.0) {
            // Moving in positive direction (VoxelShape.java line 279)
            for (int p = endIndex + 1; p < o; p++) {
                for (int q = k; q < l; q++) {
                    for (int r = m; r < n; r++) {
                        if (voxels->inBoundsAndContains(axisCycleOpposite, p, q, r)) {
                            double f = getPointPosition(axis, p) - boxMax;
                            if (f >= -1.0E-7) {
                                maxDist = std::min(maxDist, f);
                            }
                            return maxDist;
                        }
                    }
                }
            }
        } else if (maxDist < 0.0) {
            // Moving in negative direction (VoxelShape.java line 294)
            for (int p = startIndex - 1; p >= 0; p--) {
                for (int q = k; q < l; q++) {
                    for (int r = m; r < n; r++) {
                        if (voxels->inBoundsAndContains(axisCycleOpposite, p, q, r)) {
                            double f = getPointPosition(axis, p + 1) - boxMin;
                            if (f <= 1.0E-7) {
                                maxDist = std::max(maxDist, f);
                            }
                            return maxDist;
                        }
                    }
                }
            }
        }

        return maxDist;
    }
};

} // namespace FarHorizon