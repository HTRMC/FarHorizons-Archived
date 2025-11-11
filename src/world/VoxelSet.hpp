#pragma once

#include <vector>
#include <cstdint>
#include <memory>

namespace FarHorizon {

// Forward declarations
enum class Axis : uint8_t { X, Y, Z };

// ============================================================================
// VoxelSet - 3D Voxel Grid
// ============================================================================
// Stores a 3D grid of boolean values (voxel occupancy)
// Used for precise geometric culling comparisons
class VoxelSet {
public:
    VoxelSet(int sizeX, int sizeY, int sizeZ);
    virtual ~VoxelSet() = default;

    // Check if a voxel is set (occupied)
    virtual bool contains(int x, int y, int z) const = 0;

    // Set a voxel (mark as occupied)
    virtual void set(int x, int y, int z) = 0;

    // Check if voxel is in bounds AND set
    bool inBoundsAndContains(int x, int y, int z) const;

    // Check if the voxel set is empty (no voxels set)
    virtual bool isEmpty() const;

    // Get the minimum coordinate where a voxel is set (for each axis)
    virtual int getMin(Axis axis) const = 0;

    // Get the maximum coordinate where a voxel is set + 1 (for each axis)
    virtual int getMax(Axis axis) const = 0;

    // Get grid size along an axis
    int getSize(Axis axis) const;

    // Grid dimensions
    int getSizeX() const { return m_sizeX; }
    int getSizeY() const { return m_sizeY; }
    int getSizeZ() const { return m_sizeZ; }

protected:
    int m_sizeX;
    int m_sizeY;
    int m_sizeZ;
};

// ============================================================================
// VoxelShape Comparison Functions
// ============================================================================

// Check if shape1 has any voxel that shape2 doesn't have (ONLY_FIRST predicate)
// Returns true if any part of shape1 is NOT covered by shape2
// This is used for face culling: if true → draw face (exposed), if false → cull face (covered)
bool matchesAnywhere(const VoxelSet& shape1, const VoxelSet& shape2);

// ============================================================================
// BitSetVoxelSet - BitSet-based VoxelSet
// ============================================================================
// Memory-efficient storage using bit array (1 bit per voxel)
class BitSetVoxelSet : public VoxelSet {
public:
    BitSetVoxelSet(int sizeX, int sizeY, int sizeZ);

    // Factory method to create a filled box region
    static std::shared_ptr<BitSetVoxelSet> createBox(
        int sizeX, int sizeY, int sizeZ,
        int minX, int minY, int minZ,
        int maxX, int maxY, int maxZ
    );

    bool contains(int x, int y, int z) const override;
    void set(int x, int y, int z) override;
    bool isEmpty() const override;

    int getMin(Axis axis) const override;
    int getMax(Axis axis) const override;

private:
    // Set a voxel with optional bounds update
    void set(int x, int y, int z, bool updateBounds);

    // Convert 3D coordinates to 1D index
    // Formula: (x * sizeY + y) * sizeZ + z
    int getIndex(int x, int y, int z) const;

    // Bit storage (1 bit per voxel)
    // Using vector<uint8_t> where each byte stores 8 voxels
    std::vector<uint8_t> m_storage;

    // Cached bounds for optimization
    int m_minX, m_minY, m_minZ;
    int m_maxX, m_maxY, m_maxZ;
};

// ============================================================================
// SlicedVoxelSet - View wrapper that extracts a face slice
// ============================================================================
// Provides a view into a parent VoxelSet, extracting only voxels at a specific slice
class SlicedVoxelSet : public VoxelSet {
public:
    // Create a slice view along a specific axis at a given index
    // For example: axis=Y, sliceIndex=0 extracts the bottom face (y=0)
    SlicedVoxelSet(std::shared_ptr<VoxelSet> parent, Axis axis, int sliceIndex);

    bool contains(int x, int y, int z) const override;
    void set(int x, int y, int z) override;
    bool isEmpty() const override;

    int getMin(Axis axis) const override;
    int getMax(Axis axis) const override;

private:
    std::shared_ptr<VoxelSet> m_parent;
    int m_minX, m_minY, m_minZ;
    int m_maxX, m_maxY, m_maxZ;

    // Clamp a value to the cropped region and convert to local coordinates
    int clamp(Axis axis, int value) const;
};

} // namespace FarHorizon
