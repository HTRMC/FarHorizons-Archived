#pragma once

#include "VoxelSet.hpp"
#include "util/Direction.hpp"
#include <memory>
#include <algorithm>

namespace FarHorizon {

// Cropped voxel set - a view into a parent voxel set (from Minecraft's CroppedVoxelSet.java)
class CroppedVoxelSet : public VoxelSet {
private:
    std::shared_ptr<VoxelSet> parent;
    int minX, minY, minZ;
    int maxX, maxY, maxZ;

public:
    // Constructor (CroppedVoxelSet.java line 15)
    CroppedVoxelSet(std::shared_ptr<VoxelSet> parent,
                    int minX, int minY, int minZ,
                    int maxX, int maxY, int maxZ)
        : VoxelSet(maxX - minX, maxY - minY, maxZ - minZ)
        , parent(parent)
        , minX(minX), minY(minY), minZ(minZ)
        , maxX(maxX), maxY(maxY), maxZ(maxZ)
    {}

    // Check if voxel is set (CroppedVoxelSet.java line 27)
    bool contains(int x, int y, int z) const override {
        return parent->contains(minX + x, minY + y, minZ + z);
    }

    // Set a voxel (CroppedVoxelSet.java line 32)
    void set(int x, int y, int z) override {
        parent->set(minX + x, minY + y, minZ + z);
    }

    // Check if empty
    bool isEmpty() const override {
        return parent->isEmpty();
    }

    // Get minimum coordinate along axis (CroppedVoxelSet.java line 37)
    int getMin(Direction::Axis axis) const override {
        return clamp(axis, parent->getMin(axis));
    }

    // Get maximum coordinate along axis (CroppedVoxelSet.java line 42)
    int getMax(Direction::Axis axis) const override {
        return clamp(axis, parent->getMax(axis));
    }

private:
    // Clamp value to cropped region (CroppedVoxelSet.java line 46)
    int clamp(Direction::Axis axis, int value) const {
        int min = Direction::choose(axis, minX, minY, minZ);
        int max = Direction::choose(axis, maxX, maxY, maxZ);
        return std::clamp(value, min, max) - min;
    }
};

} // namespace FarHorizon