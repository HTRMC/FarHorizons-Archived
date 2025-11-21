#pragma once

#include "VoxelSet.hpp"
#include "util/Direction.hpp"
#include <functional>
#include <memory>

namespace FarHorizon {

// Minecraft: DiscreteVoxelShape.java
// Represents a discrete voxel grid and provides edge/face iteration
class DiscreteVoxelShape {
public:
    using IntLineConsumer = std::function<void(int x1, int y1, int z1, int x2, int y2, int z2)>;

    explicit DiscreteVoxelShape(std::shared_ptr<VoxelSet> voxels)
        : voxels_(voxels)
        , xSize_(voxels->getXSize())
        , ySize_(voxels->getYSize())
        , zSize_(voxels->getZSize())
    {}

    // Check if voxel is filled, with bounds checking (Minecraft line 66-72)
    bool isFullWide(int x, int y, int z) const {
        if (x >= 0 && y >= 0 && z >= 0) {
            return x < xSize_ && y < ySize_ && z < zSize_ ? voxels_->contains(x, y, z) : false;
        }
        return false;
    }

    // Minecraft: AxisCycle (simplified - we only need the 3 basic cycles)
    enum class AxisCycle {
        NONE,     // X, Y, Z
        FORWARD,  // Z, X, Y
        BACKWARD  // Y, Z, X
    };

    // Apply axis cycle transformation to coordinates
    static int cycle(AxisCycle transform, int x, int y, int z, Direction::Axis axis) {
        switch (transform) {
            case AxisCycle::NONE:
                return Direction::choose(axis, x, y, z);
            case AxisCycle::FORWARD:
                // Z->X, X->Y, Y->Z
                return Direction::choose(axis, z, x, y);
            case AxisCycle::BACKWARD:
                // Y->X, Z->Y, X->Z
                return Direction::choose(axis, y, z, x);
        }
        return 0;
    }

    // Get grid size after axis transformation
    int getSize(AxisCycle transform, Direction::Axis axis) const {
        switch (transform) {
            case AxisCycle::NONE:
                return Direction::choose(axis, xSize_, ySize_, zSize_);
            case AxisCycle::FORWARD:
                return Direction::choose(axis, zSize_, xSize_, ySize_);
            case AxisCycle::BACKWARD:
                return Direction::choose(axis, ySize_, zSize_, xSize_);
        }
        return 0;
    }

    // Iterate over all edges (Minecraft DiscreteVoxelShape.java lines 162-207)
    void forAllEdges(const IntLineConsumer& consumer, bool mergeNeighbors) const {
        forAllAxisEdges(consumer, AxisCycle::NONE, mergeNeighbors);
        forAllAxisEdges(consumer, AxisCycle::FORWARD, mergeNeighbors);
        forAllAxisEdges(consumer, AxisCycle::BACKWARD, mergeNeighbors);
    }

private:
    std::shared_ptr<VoxelSet> voxels_;
    int xSize_;
    int ySize_;
    int zSize_;

    // Iterate over edges for a specific axis orientation (Minecraft lines 168-207)
    void forAllAxisEdges(const IntLineConsumer& consumer, AxisCycle transform, bool mergeNeighbors) const {
        // Get inverse transformation for coordinate conversion
        AxisCycle inverseTransform = getInverse(transform);

        // Get grid dimensions in transformed space
        int sizeX = getSize(inverseTransform, Direction::Axis::X);
        int sizeY = getSize(inverseTransform, Direction::Axis::Y);
        int sizeZ = getSize(inverseTransform, Direction::Axis::Z);

        // Iterate over all grid positions (including boundaries)
        // Minecraft lines 174-206
        for (int x = 0; x <= sizeX; ++x) {
            for (int y = 0; y <= sizeY; ++y) {
                int edgeStart = -1;

                for (int z = 0; z <= sizeZ; ++z) {
                    // Check 2×2 neighborhood pattern (Minecraft lines 179-189)
                    int filledCount = 0;
                    int parity = 0;

                    for (int dx = 0; dx <= 1; ++dx) {
                        for (int dy = 0; dy <= 1; ++dy) {
                            // Check if voxel at (x+dx-1, y+dy-1, z) is filled
                            if (isFullWideTransformed(inverseTransform, x + dx - 1, y + dy - 1, z)) {
                                ++filledCount;
                                parity ^= dx ^ dy;
                            }
                        }
                    }

                    // Determine if there's an edge at this position (Minecraft lines 191-202)
                    // Edge exists if:
                    // - filledCount == 1 (single filled voxel)
                    // - filledCount == 3 (three filled voxels)
                    // - filledCount == 2 && (parity & 1) == 0 (two filled voxels with even parity)
                    if (filledCount == 1 || filledCount == 3 || (filledCount == 2 && (parity & 1) == 0)) {
                        if (mergeNeighbors) {
                            // Start or continue edge segment
                            if (edgeStart == -1) {
                                edgeStart = z;
                            }
                        } else {
                            // Emit individual edge segment
                            consumer(
                                cycle(inverseTransform, x, y, z, Direction::Axis::X),
                                cycle(inverseTransform, x, y, z, Direction::Axis::Y),
                                cycle(inverseTransform, x, y, z, Direction::Axis::Z),
                                cycle(inverseTransform, x, y, z + 1, Direction::Axis::X),
                                cycle(inverseTransform, x, y, z + 1, Direction::Axis::Y),
                                cycle(inverseTransform, x, y, z + 1, Direction::Axis::Z)
                            );
                        }
                    } else if (edgeStart != -1) {
                        // End of edge segment - emit merged edge
                        consumer(
                            cycle(inverseTransform, x, y, edgeStart, Direction::Axis::X),
                            cycle(inverseTransform, x, y, edgeStart, Direction::Axis::Y),
                            cycle(inverseTransform, x, y, edgeStart, Direction::Axis::Z),
                            cycle(inverseTransform, x, y, z, Direction::Axis::X),
                            cycle(inverseTransform, x, y, z, Direction::Axis::Y),
                            cycle(inverseTransform, x, y, z, Direction::Axis::Z)
                        );
                        edgeStart = -1;
                    }
                }
            }
        }
    }

    // Check if voxel is filled with axis transformation
    bool isFullWideTransformed(AxisCycle transform, int x, int y, int z) const {
        int actualX = cycle(transform, x, y, z, Direction::Axis::X);
        int actualY = cycle(transform, x, y, z, Direction::Axis::Y);
        int actualZ = cycle(transform, x, y, z, Direction::Axis::Z);
        return isFullWide(actualX, actualY, actualZ);
    }

    // Get inverse axis cycle transformation
    static AxisCycle getInverse(AxisCycle transform) {
        switch (transform) {
            case AxisCycle::NONE: return AxisCycle::NONE;
            case AxisCycle::FORWARD: return AxisCycle::BACKWARD;
            case AxisCycle::BACKWARD: return AxisCycle::FORWARD;
        }
        return AxisCycle::NONE;
    }
};

} // namespace FarHorizon
