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

// Helper struct for merged coordinate indices (like Minecraft's PairList)
struct IndexPair {
    int index1;  // Index in shape1
    int index2;  // Index in shape2
};

// Merge coordinate grids from two shapes (like Minecraft's createListPair + SimplePairList)
// Returns pairs of indices that represent the same voxel in both coordinate systems
static std::vector<IndexPair> mergeCoordinateGrid(int size1, int size2, int min1, int max1, int min2, int max2) {
    std::vector<IndexPair> merged;

    // Special case: identical grids (like Minecraft's IdentityPairList)
    if (size1 == size2 && min1 == min2 && max1 == max2) {
        for (int i = min1; i < max1; i++) {
            merged.push_back({i, i});
        }
        return merged;
    }

    // General case: merge two different grids using normalized coordinates
    // Map each voxel index to normalized 0-1 space, then merge
    //
    // Minecraft does this with DoubleList + SimplePairList
    // We'll do a simplified version that achieves the same result

    // Collect all unique normalized coordinates from both shapes
    std::vector<std::pair<double, IndexPair>> normalized;

    // Add coordinates from shape1
    for (int i = min1; i < max1; i++) {
        double normCoord = static_cast<double>(i) / size1;
        // Find corresponding index in shape2
        int i2 = static_cast<int>(normCoord * size2);
        i2 = std::max(min2, std::min(max2 - 1, i2));
        normalized.push_back({normCoord, {i, i2}});
    }

    // Add coordinates from shape2
    for (int i = min2; i < max2; i++) {
        double normCoord = static_cast<double>(i) / size2;
        // Find corresponding index in shape1
        int i1 = static_cast<int>(normCoord * size1);
        i1 = std::max(min1, std::min(max1 - 1, i1));
        normalized.push_back({normCoord, {i1, i}});
    }

    // Sort by normalized coordinate
    std::sort(normalized.begin(), normalized.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    // Remove duplicates (coordinates within epsilon)
    constexpr double EPSILON = 1.0e-7;
    for (size_t i = 0; i < normalized.size(); i++) {
        if (i > 0 && std::abs(normalized[i].first - normalized[i-1].first) < EPSILON) {
            continue;  // Skip duplicate
        }
        merged.push_back(normalized[i].second);
    }

    return merged;
}

bool matchesAnywhere(const VoxelSet& shape1, const VoxelSet& shape2) {
    // Minecraft's logic: check if shape1 has any voxel that shape2 doesn't have
    //
    // Like Minecraft's VoxelShapes.matchesAnywhere(shape1, shape2, BooleanBiFunction.ONLY_FIRST)
    // which tests: shape1.contains(voxel) && !shape2.contains(voxel)

    // Early exit: if shape1 is empty, nothing is exposed
    if (shape1.isEmpty()) {
        return false;
    }

    // Early exit: if shape2 is empty, shape1 is fully exposed
    if (shape2.isEmpty()) {
        return true;
    }

    // Get dimensions
    int size1X = shape1.getSize(Axis::X);
    int size1Y = shape1.getSize(Axis::Y);
    int size1Z = shape1.getSize(Axis::Z);
    int size2X = shape2.getSize(Axis::X);
    int size2Y = shape2.getSize(Axis::Y);
    int size2Z = shape2.getSize(Axis::Z);

    // Get bounds
    int min1X = shape1.getMin(Axis::X);
    int max1X = shape1.getMax(Axis::X);
    int min1Y = shape1.getMin(Axis::Y);
    int max1Y = shape1.getMax(Axis::Y);
    int min1Z = shape1.getMin(Axis::Z);
    int max1Z = shape1.getMax(Axis::Z);

    int min2X = shape2.getMin(Axis::X);
    int max2X = shape2.getMax(Axis::X);
    int min2Y = shape2.getMin(Axis::Y);
    int max2Y = shape2.getMax(Axis::Y);
    int min2Z = shape2.getMin(Axis::Z);
    int max2Z = shape2.getMax(Axis::Z);

    // Create merged coordinate grids for each axis (like Minecraft's PairList)
    auto mergedX = mergeCoordinateGrid(size1X, size2X, min1X, max1X, min2X, max2X);
    auto mergedY = mergeCoordinateGrid(size1Y, size2Y, min1Y, max1Y, min2Y, max2Y);
    auto mergedZ = mergeCoordinateGrid(size1Z, size2Z, min1Z, max1Z, min2Z, max2Z);

    // Iterate through merged grid (like Minecraft's triple-nested forEachPair)
    // For each merged voxel, check: shape1.contains(x1,y1,z1) && !shape2.contains(x2,y2,z2)
    for (const auto& xPair : mergedX) {
        for (const auto& yPair : mergedY) {
            for (const auto& zPair : mergedZ) {
                bool inShape1 = shape1.contains(xPair.index1, yPair.index1, zPair.index1);
                bool inShape2 = shape2.inBoundsAndContains(xPair.index2, yPair.index2, zPair.index2);

                // ONLY_FIRST predicate: shape1 && !shape2
                if (inShape1 && !inShape2) {
                    return true;  // Found exposed voxel
                }
            }
        }
    }

    // All voxels in shape1 are also in shape2 → shape1 is fully covered
    return false;
}

// ============================================================================
// SlicedVoxelSet Implementation (like Minecraft's CroppedVoxelSet)
// ============================================================================

SlicedVoxelSet::SlicedVoxelSet(std::shared_ptr<VoxelSet> parent, Axis axis, int sliceIndex)
    : VoxelSet(parent->getSizeX(), parent->getSizeY(), parent->getSizeZ()),
      m_parent(parent) {

    // Initialize bounds to parent's size (uncropped)
    m_minX = 0;
    m_minY = 0;
    m_minZ = 0;
    m_maxX = parent->getSizeX();
    m_maxY = parent->getSizeY();
    m_maxZ = parent->getSizeZ();

    // Crop to the specified slice (like Minecraft's SlicedVoxelShape)
    // We extract a single layer perpendicular to the axis
    switch (axis) {
        case Axis::X:
            m_minX = sliceIndex;
            m_maxX = sliceIndex + 1;
            break;
        case Axis::Y:
            m_minY = sliceIndex;
            m_maxY = sliceIndex + 1;
            break;
        case Axis::Z:
            m_minZ = sliceIndex;
            m_maxZ = sliceIndex + 1;
            break;
    }
}

bool SlicedVoxelSet::contains(int x, int y, int z) const {
    // Offset coordinates to parent's space (like Minecraft's CroppedVoxelSet)
    return m_parent->inBoundsAndContains(m_minX + x, m_minY + y, m_minZ + z);
}

void SlicedVoxelSet::set(int x, int y, int z) {
    // Offset coordinates to parent's space
    m_parent->set(m_minX + x, m_minY + y, m_minZ + z);
}

bool SlicedVoxelSet::isEmpty() const {
    // Delegate to parent, checking only within cropped region
    for (int x = m_minX; x < m_maxX; x++) {
        for (int y = m_minY; y < m_maxY; y++) {
            for (int z = m_minZ; z < m_maxZ; z++) {
                if (m_parent->contains(x, y, z)) {
                    return false;
                }
            }
        }
    }
    return true;
}

int SlicedVoxelSet::getMin(Axis axis) const {
    // Like Minecraft's CroppedVoxelSet.getMin()
    // Returns the minimum coordinate within the cropped region
    return clamp(axis, m_parent->getMin(axis));
}

int SlicedVoxelSet::getMax(Axis axis) const {
    // Like Minecraft's CroppedVoxelSet.getMax()
    // Returns the maximum coordinate within the cropped region
    return clamp(axis, m_parent->getMax(axis));
}

int SlicedVoxelSet::clamp(Axis axis, int value) const {
    // Like Minecraft's CroppedVoxelSet.clampIndex()
    // Clamps a value from parent's coordinate space to local cropped space
    int min, max;
    switch (axis) {
        case Axis::X:
            min = m_minX;
            max = m_maxX;
            break;
        case Axis::Y:
            min = m_minY;
            max = m_maxY;
            break;
        case Axis::Z:
            min = m_minZ;
            max = m_maxZ;
            break;
    }

    // Clamp to cropped region and convert to local coordinates (0-based)
    return std::max(0, std::min(max, value) - min);
}

} // namespace FarHorizon
