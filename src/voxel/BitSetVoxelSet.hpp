#pragma once

#include "VoxelSet.hpp"
#include <vector>
#include <algorithm>

namespace FarHorizon {

// BitSet-based voxel grid implementation (from Minecraft's BitSetVoxelSet.java)
class BitSetVoxelSet : public VoxelSet {
private:
    std::vector<bool> storage;  // std::vector<bool> is bit-packed like Java's BitSet
    int minX, minY, minZ;
    int maxX, maxY, maxZ;

public:
    // Constructor (BitSetVoxelSet.java line 16)
    BitSetVoxelSet(int sizeX, int sizeY, int sizeZ)
        : VoxelSet(sizeX, sizeY, sizeZ)
        , storage(sizeX * sizeY * sizeZ, false)
        , minX(sizeX), minY(sizeY), minZ(sizeZ)
        , maxX(0), maxY(0), maxZ(0)
    {}

    // Factory method to create with specific bounds (BitSetVoxelSet.java line 24)
    static BitSetVoxelSet create(int sizeX, int sizeY, int sizeZ,
                                  int minX, int minY, int minZ,
                                  int maxX, int maxY, int maxZ) {
        BitSetVoxelSet voxelSet(sizeX, sizeY, sizeZ);
        voxelSet.minX = minX;
        voxelSet.minY = minY;
        voxelSet.minZ = minZ;
        voxelSet.maxX = maxX;
        voxelSet.maxY = maxY;
        voxelSet.maxZ = maxZ;

        // Fill the region
        for (int x = minX; x < maxX; x++) {
            for (int y = minY; y < maxY; y++) {
                for (int z = minZ; z < maxZ; z++) {
                    voxelSet.setInternal(x, y, z, false);
                }
            }
        }

        return voxelSet;
    }

    // Convert 3D coordinates to 1D index (BitSetVoxelSet.java line 70)
    int getIndex(int x, int y, int z) const {
        return (x * sizeY + y) * sizeZ + z;
    }

    // Check if voxel is set (BitSetVoxelSet.java line 75)
    bool contains(int x, int y, int z) const override {
        return storage[getIndex(x, y, z)];
    }

    // Set a voxel (BitSetVoxelSet.java line 92)
    void set(int x, int y, int z) override {
        setInternal(x, y, z, true);
    }

    // Check if empty (BitSetVoxelSet.java line 97)
    bool isEmpty() const override {
        // Check if all storage is false
        return std::none_of(storage.begin(), storage.end(), [](bool b) { return b; });
    }

    // Get minimum coordinate along axis (BitSetVoxelSet.java line 102)
    int getMin(Direction::Axis axis) const override {
        return Direction::choose(axis, minX, minY, minZ);
    }

    // Get maximum coordinate along axis (BitSetVoxelSet.java line 107)
    int getMax(Direction::Axis axis) const override {
        return Direction::choose(axis, maxX, maxY, maxZ);
    }

private:
    // Internal set with optional bounds update (BitSetVoxelSet.java line 79)
    void setInternal(int x, int y, int z, bool updateBounds) {
        storage[getIndex(x, y, z)] = true;
        if (updateBounds) {
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            minZ = std::min(minZ, z);
            maxX = std::max(maxX, x + 1);
            maxY = std::max(maxY, y + 1);
            maxZ = std::max(maxZ, z + 1);
        }
    }
};

} // namespace FarHorizon