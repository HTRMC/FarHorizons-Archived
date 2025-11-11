#include "VoxelSet.hpp"
#include <stdexcept>
#include <algorithm>
#include <string>

namespace FarHorizon {

// ============================================================================
// VoxelSet Implementation
// ============================================================================

VoxelSet::VoxelSet(int sizeX, int sizeY, int sizeZ)
    : m_sizeX(sizeX), m_sizeY(sizeY), m_sizeZ(sizeZ) {
    if (sizeX < 0 || sizeY < 0 || sizeZ < 0) {
        throw std::invalid_argument("Need all positive sizes: x: " + std::to_string(sizeX) +
                                    ", y: " + std::to_string(sizeY) + ", z: " + std::to_string(sizeZ));
    }
}

bool VoxelSet::inBoundsAndContains(int x, int y, int z) const {
    if (x < 0 || y < 0 || z < 0) {
        return false;
    }
    if (x >= m_sizeX || y >= m_sizeY || z >= m_sizeZ) {
        return false;
    }
    return contains(x, y, z);
}

bool VoxelSet::isEmpty() const {
    // Check if any axis has min >= max (no voxels set)
    if (getMin(Axis::X) >= getMax(Axis::X)) return true;
    if (getMin(Axis::Y) >= getMax(Axis::Y)) return true;
    if (getMin(Axis::Z) >= getMax(Axis::Z)) return true;
    return false;
}

int VoxelSet::getSize(Axis axis) const {
    switch (axis) {
        case Axis::X: return m_sizeX;
        case Axis::Y: return m_sizeY;
        case Axis::Z: return m_sizeZ;
    }
    return 0;
}

// ============================================================================
// BitSetVoxelSet Implementation
// ============================================================================

BitSetVoxelSet::BitSetVoxelSet(int sizeX, int sizeY, int sizeZ)
    : VoxelSet(sizeX, sizeY, sizeZ),
      m_minX(sizeX), m_minY(sizeY), m_minZ(sizeZ),
      m_maxX(0), m_maxY(0), m_maxZ(0) {

    // Calculate number of bytes needed (8 bits per byte)
    int totalVoxels = sizeX * sizeY * sizeZ;
    int numBytes = (totalVoxels + 7) / 8;  // Round up
    m_storage.resize(numBytes, 0);
}

std::shared_ptr<BitSetVoxelSet> BitSetVoxelSet::createBox(
    int sizeX, int sizeY, int sizeZ,
    int minX, int minY, int minZ,
    int maxX, int maxY, int maxZ
) {
    auto voxelSet = std::make_shared<BitSetVoxelSet>(sizeX, sizeY, sizeZ);

    // Set bounds
    voxelSet->m_minX = minX;
    voxelSet->m_minY = minY;
    voxelSet->m_minZ = minZ;
    voxelSet->m_maxX = maxX;
    voxelSet->m_maxY = maxY;
    voxelSet->m_maxZ = maxZ;

    // Fill the box region (like Minecraft's BitSetVoxelSet.create)
    for (int x = minX; x < maxX; x++) {
        for (int y = minY; y < maxY; y++) {
            for (int z = minZ; z < maxZ; z++) {
                voxelSet->set(x, y, z, false);  // Don't update bounds (already set)
            }
        }
    }

    return voxelSet;
}

int BitSetVoxelSet::getIndex(int x, int y, int z) const {
    // Minecraft's formula: (x * sizeY + y) * sizeZ + z
    return (x * m_sizeY + y) * m_sizeZ + z;
}

bool BitSetVoxelSet::contains(int x, int y, int z) const {
    int index = getIndex(x, y, z);
    int byteIndex = index / 8;
    int bitIndex = index % 8;
    return (m_storage[byteIndex] & (1 << bitIndex)) != 0;
}

void BitSetVoxelSet::set(int x, int y, int z, bool updateBounds) {
    int index = getIndex(x, y, z);
    int byteIndex = index / 8;
    int bitIndex = index % 8;

    m_storage[byteIndex] |= (1 << bitIndex);

    if (updateBounds) {
        m_minX = std::min(m_minX, x);
        m_minY = std::min(m_minY, y);
        m_minZ = std::min(m_minZ, z);
        m_maxX = std::max(m_maxX, x + 1);
        m_maxY = std::max(m_maxY, y + 1);
        m_maxZ = std::max(m_maxZ, z + 1);
    }
}

void BitSetVoxelSet::set(int x, int y, int z) {
    set(x, y, z, true);
}

bool BitSetVoxelSet::isEmpty() const {
    // Check if any bit is set
    for (uint8_t byte : m_storage) {
        if (byte != 0) return false;
    }
    return true;
}

int BitSetVoxelSet::getMin(Axis axis) const {
    switch (axis) {
        case Axis::X: return m_minX;
        case Axis::Y: return m_minY;
        case Axis::Z: return m_minZ;
    }
    return 0;
}

int BitSetVoxelSet::getMax(Axis axis) const {
    switch (axis) {
        case Axis::X: return m_maxX;
        case Axis::Y: return m_maxY;
        case Axis::Z: return m_maxZ;
    }
    return 0;
}

// ============================================================================
// matchesAnywhere Implementation (ONLY_FIRST predicate)
// ============================================================================

bool matchesAnywhere(const VoxelSet& shape1, const VoxelSet& shape2) {
    // Minecraft's logic: check if shape1 has any voxel that shape2 doesn't have
    //
    // Simplified version: iterate through all voxels in shape1's bounds
    // If we find any voxel where:
    //   - shape1 contains it AND
    //   - shape2 does NOT contain it
    // Then return true (shape1 is exposed)
    //
    // This is equivalent to Minecraft's:
    //   matchesAnywhere(shape1, shape2, BooleanBiFunction.ONLY_FIRST)
    // which tests: shape1.contains(voxel) && !shape2.contains(voxel)

    // Early exit: if shape1 is empty, nothing is exposed
    if (shape1.isEmpty()) {
        return false;
    }

    // Early exit: if shape2 is empty, shape1 is fully exposed
    if (shape2.isEmpty()) {
        return true;
    }

    // Get the bounds of shape1 (only need to check where shape1 has voxels)
    int minX = shape1.getMin(Axis::X);
    int maxX = shape1.getMax(Axis::X);
    int minY = shape1.getMin(Axis::Y);
    int maxY = shape1.getMax(Axis::Y);
    int minZ = shape1.getMin(Axis::Z);
    int maxZ = shape1.getMax(Axis::Z);

    // Iterate through shape1's bounding box
    for (int x = minX; x < maxX; x++) {
        for (int y = minY; y < maxY; y++) {
            for (int z = minZ; z < maxZ; z++) {
                // Check if shape1 has a voxel here
                if (shape1.contains(x, y, z)) {
                    // Check if shape2 also has a voxel here
                    // NOTE: shape2 might have different dimensions, so we use inBoundsAndContains
                    if (!shape2.inBoundsAndContains(x, y, z)) {
                        // Found a voxel in shape1 that's not in shape2!
                        // Shape1 is exposed at this position
                        return true;
                    }
                }
            }
        }
    }

    // All voxels in shape1 are also in shape2 → shape1 is fully covered
    return false;
}

} // namespace FarHorizon
