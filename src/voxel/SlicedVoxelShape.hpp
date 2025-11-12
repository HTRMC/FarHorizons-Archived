#pragma once

#include "VoxelShape.hpp"
#include "CroppedVoxelSet.hpp"
#include "util/Direction.hpp"
#include <memory>
#include <vector>

namespace FarHorizon {

// Sliced voxel shape - represents a single slice of a shape (from Minecraft's SlicedVoxelShape.java)
class SlicedVoxelShape : public VoxelShape {
private:
    std::shared_ptr<VoxelShape> shape;
    Direction::Axis axis;

    // Static POINTS for sliced axis: [0.0, 1.0] (from FractionalDoubleList(1))
    static const std::vector<double> POINTS;

public:
    // Constructor (SlicedVoxelShape.java line 11)
    SlicedVoxelShape(std::shared_ptr<VoxelShape> shape, Direction::Axis axis, int sliceWidth)
        : VoxelShape(createVoxelSet(shape->getVoxels(), axis, sliceWidth))
        , shape(shape)
        , axis(axis)
    {}

    // Get point positions along an axis (SlicedVoxelShape.java line 30)
    const std::vector<double>& getPointPositions(Direction::Axis requestedAxis) const override {
        // For the sliced axis, return [0.0, 1.0]
        // For other axes, delegate to the parent shape
        return requestedAxis == axis ? POINTS : shape->getPointPositions(requestedAxis);
    }

private:
    // Create a cropped voxel set for the slice (SlicedVoxelShape.java line 17)
    static std::shared_ptr<VoxelSet> createVoxelSet(const VoxelSet& voxelSet, Direction::Axis axis, int sliceWidth) {
        // Extract the dimensions
        int sizeX = voxelSet.getXSize();
        int sizeY = voxelSet.getYSize();
        int sizeZ = voxelSet.getZSize();

        // Create cropped bounds (SlicedVoxelShape.java line 18-26)
        int minX = Direction::choose(axis, sliceWidth, 0, 0);
        int minY = Direction::choose(axis, 0, sliceWidth, 0);
        int minZ = Direction::choose(axis, 0, 0, sliceWidth);
        int maxX = Direction::choose(axis, sliceWidth + 1, sizeX, sizeX);
        int maxY = Direction::choose(axis, sizeY, sliceWidth + 1, sizeY);
        int maxZ = Direction::choose(axis, sizeZ, sizeZ, sliceWidth + 1);

        // We need to create a shared_ptr from the const reference
        // This is a view, so we wrap it - note this assumes the voxelSet outlives the slice
        auto parentPtr = std::shared_ptr<VoxelSet>(const_cast<VoxelSet*>(&voxelSet), [](VoxelSet*){});

        return std::make_shared<CroppedVoxelSet>(parentPtr, minX, minY, minZ, maxX, maxY, maxZ);
    }
};

// Initialize static POINTS: [0.0, 1.0] (FractionalDoubleList(1) creates {0.0, 1.0})
inline const std::vector<double> SlicedVoxelShape::POINTS = {0.0, 1.0};

} // namespace FarHorizon