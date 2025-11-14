#pragma once

#include "VoxelSet.hpp"
#include "physics/AABB.hpp"
#include "util/Mth.hpp"
#include <vector>
#include <memory>
#include <spdlog/spdlog.h>

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

    // Move the voxel shape by an offset (Minecraft: VoxelShape move(Vec3i offset))
    // Returns a new VoxelShape translated by the given offset
    std::shared_ptr<VoxelShape> move(const glm::ivec3& offset) const;

    // Move the voxel shape by an offset (double version)
    std::shared_ptr<VoxelShape> move(double x, double y, double z) const {
        return move(glm::ivec3(static_cast<int>(x), static_cast<int>(y), static_cast<int>(z)));
    }

    // Get position of a point index along an axis (VoxelShape.java line 67)
    double getPointPosition(Direction::Axis axis, int index) const {
        return getPointPositions(axis)[index];
    }

    // Find the voxel index containing a coordinate (VoxelShape.java line 123: findIndex)
    // Minecraft: Mth.binarySearch(0, shape.getSize(axis) + 1, i -> coord < get(axis, i)) - 1
    // Returns the voxel index containing the position
    // Can return -1 (before shape) or size (after shape) for out-of-bounds positions
    int findIndex(Direction::Axis axis, double coord) const {
        // Binary search: find first index where coord < getPointPosition(axis, i)
        return Mth::binarySearch(0, voxels->getSize(axis) + 1, [this, axis, coord](int i) {
            return coord < this->getPointPosition(axis, i);
        }) - 1;
    }

    // Calculate maximum distance before collision (VoxelShape.java line 217: collide)
    // This is the CORE collision method!
    double collide(Direction::Axis axis, const AABB& moving, double distance) const {
        return collideX(AxisCycle::between(Direction::Axis::X, axis), moving, distance);
    }

protected:
    // Internal collision with axis cycling (VoxelShape.java line 221: collideX)
    double collideX(AxisCycleDirection axisCycle, const AABB& moving, double distance) const {
        if (isEmpty()) {
            return distance;
        }
        if (std::abs(distance) < 1.0E-7) {
            return 0.0;
        }

        // Apply the axis cycle transformation (VoxelShape.java line 227: var5 = transform.inverse())
        // The inverse cycle is used for voxel indexing, but the forward cycle is used for axis selection
        AxisCycleDirection axisCycleOpposite = AxisCycle::opposite(axisCycle);
        Direction::Axis axis = AxisCycle::apply(axisCycle, Direction::Axis::X);
        Direction::Axis axis2 = AxisCycle::apply(axisCycle, Direction::Axis::Y);
        Direction::Axis axis3 = AxisCycle::apply(axisCycle, Direction::Axis::Z);

        // Get box bounds along the cycled axes (VoxelShape.java line 231-232)
        double var9 = Direction::choose(axis, moving.maxX, moving.maxY, moving.maxZ);
        double var11 = Direction::choose(axis, moving.minX, moving.minY, moving.minZ);

        // Get voxel indices (VoxelShape.java line 233-234)
        int var13 = findIndex(axis, var11 + 1.0E-7);
        int var14 = findIndex(axis, var9 - 1.0E-7);

        // Get bounds for other axes (VoxelShape.java line 235-238)
        double axis2Min = Direction::choose(axis2, moving.minX, moving.minY, moving.minZ);
        double axis2Max = Direction::choose(axis2, moving.maxX, moving.maxY, moving.maxZ);
        double axis3Min = Direction::choose(axis3, moving.minX, moving.minY, moving.minZ);
        double axis3Max = Direction::choose(axis3, moving.maxX, moving.maxY, moving.maxZ);

        // Get voxel ranges (VoxelShape.java line 235-238)
        int var15 = std::max(0, findIndex(axis2, axis2Min + 1.0E-7));
        int var16 = std::min(voxels->getSize(axis2), findIndex(axis2, axis2Max - 1.0E-7) + 1);
        int var17 = std::max(0, findIndex(axis3, axis3Min + 1.0E-7));
        int var18 = std::min(voxels->getSize(axis3), findIndex(axis3, axis3Max - 1.0E-7) + 1);
        int var19 = voxels->getSize(axis);

        // Debug: Log shape points and calculated indices
        const auto& axisPoints = getPointPositions(axis);
        const auto& axis2Points = getPointPositions(axis2);
        const auto& axis3Points = getPointPositions(axis3);

        if (distance > 0.0) {
            // Moving in positive direction (VoxelShape.java line 244)
            for (int var20 = var14 + 1; var20 < var19; var20++) {
                for (int var21 = var15; var21 < var16; var21++) {
                    for (int var22 = var17; var22 < var18; var22++) {
                        if (voxels->inBoundsAndContains(axisCycleOpposite, var20, var21, var22)) {
                            double var23 = getPointPosition(axis, var20) - var9;
                            if (var23 >= -1.0E-7) {
                                distance = std::min(distance, var23);
                            }
                            return distance;
                        }
                    }
                }
            }
        } else if (distance < 0.0) {
            // Moving in negative direction (VoxelShape.java line 259)
            for (int var20 = var13 - 1; var20 >= 0; var20--) {
                for (int var21 = var15; var21 < var16; var21++) {
                    for (int var22 = var17; var22 < var18; var22++) {
                        bool contains = voxels->inBoundsAndContains(axisCycleOpposite, var20, var21, var22);
                        if (contains) {
                            double var23 = getPointPosition(axis, var20 + 1) - var11;
                            if (var23 <= 1.0E-7) {
                                distance = std::max(distance, var23);
                            }
                            return distance;
                        }
                    }
                }
            }
        }

        return distance;
    }
};

} // namespace FarHorizon