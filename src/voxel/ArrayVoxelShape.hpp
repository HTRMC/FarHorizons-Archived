#pragma once

#include "VoxelShape.hpp"
#include <vector>
#include <memory>

namespace FarHorizon {

// Concrete voxel shape with array-based point storage (from Minecraft's ArrayVoxelShape.java)
class ArrayVoxelShape : public VoxelShape {
private:
    std::vector<double> xPoints;
    std::vector<double> yPoints;
    std::vector<double> zPoints;

public:
    // Constructor (ArrayVoxelShape.java line 23)
    ArrayVoxelShape(std::shared_ptr<VoxelSet> voxels,
                   const std::vector<double>& xPoints,
                   const std::vector<double>& yPoints,
                   const std::vector<double>& zPoints)
        : VoxelShape(voxels)
        , xPoints(xPoints)
        , yPoints(yPoints)
        , zPoints(zPoints)
    {
        // Validate sizes (ArrayVoxelShape.java line 28)
        int expectedXSize = voxels->getXSize() + 1;
        int expectedYSize = voxels->getYSize() + 1;
        int expectedZSize = voxels->getZSize() + 1;

        if (static_cast<int>(xPoints.size()) != expectedXSize ||
            static_cast<int>(yPoints.size()) != expectedYSize ||
            static_cast<int>(zPoints.size()) != expectedZSize) {
            throw std::invalid_argument("Point array sizes must match voxel dimensions + 1");
        }
    }

    // Get point positions along an axis (ArrayVoxelShape.java line 40)
    const std::vector<double>& getPointPositions(Direction::Axis axis) const override {
        switch (axis) {
            case Direction::Axis::X: return xPoints;
            case Direction::Axis::Y: return yPoints;
            case Direction::Axis::Z: return zPoints;
            default: return xPoints;
        }
    }
};

} // namespace FarHorizon