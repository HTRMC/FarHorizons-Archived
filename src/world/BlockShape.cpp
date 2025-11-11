#include "BlockShape.hpp"
#include <algorithm>
#include <cmath>

namespace FarHorizon {

// ============================================================================
// Static instances for common shapes (like Minecraft's VoxelShapes.empty() and VoxelShapes.fullCube())
// ============================================================================

static BlockShape s_emptyShape;  // Default constructed (empty)
static BlockShape s_fullCubeShape;  // Will be initialized with 1×1×1 grid

// Static initialization helper
struct StaticInitializer {
    StaticInitializer() {
        // Create full cube: 1×1×1 grid with single voxel set
        auto voxels = std::make_shared<BitSetVoxelSet>(1, 1, 1);
        voxels->set(0, 0, 0);
        s_fullCubeShape = BlockShape(voxels);
    }
};
static StaticInitializer s_init;

// ============================================================================
// BlockShape Implementation
// ============================================================================

BlockShape::BlockShape()
    : m_voxels(nullptr), m_type(Type::EMPTY) {
    // Empty shape has no voxels
    for (int i = 0; i < 6; i++) {
        m_cullingFaces[i] = nullptr;
    }
}

BlockShape::BlockShape(std::shared_ptr<VoxelSet> voxels)
    : m_voxels(voxels) {
    // Determine type
    if (!voxels || voxels->isEmpty()) {
        m_type = Type::EMPTY;
    } else if (voxels->getSizeX() == 1 && voxels->getSizeY() == 1 && voxels->getSizeZ() == 1) {
        m_type = Type::FULL_CUBE;
    } else {
        m_type = Type::PARTIAL;
    }

    // Initialize culling face cache
    for (int i = 0; i < 6; i++) {
        m_cullingFaces[i] = nullptr;
    }
}

const BlockShape& BlockShape::empty() {
    return s_emptyShape;
}

const BlockShape& BlockShape::fullCube() {
    return s_fullCubeShape;
}

// Helper: Find required bit resolution for a coordinate range (like Minecraft's VoxelShapes.findRequiredBitResolution)
// Returns 0, 1, 2, or 3 for resolutions 1, 2, 4, or 8
// Returns -1 if coordinates are out of range or don't snap to a power-of-2 grid
static int findRequiredBitResolution(float min, float max) {
    constexpr float MIN_SIZE = 1.0e-7f;
    constexpr float EPSILON = 1.0e-7f;

    // Check if coordinates are within valid range [0, 1]
    if (min < -EPSILON || max > 1.0f + EPSILON) {
        return -1;
    }

    // Try resolutions 1, 2, 4, 8 (i = 0, 1, 2, 3)
    for (int i = 0; i <= 3; i++) {
        int resolution = 1 << i;  // 2^i
        float scaledMin = min * resolution;
        float scaledMax = max * resolution;

        // Check if min and max snap to grid points
        float roundedMin = std::round(scaledMin);
        float roundedMax = std::round(scaledMax);

        bool minSnaps = std::abs(scaledMin - roundedMin) < EPSILON * resolution;
        bool maxSnaps = std::abs(scaledMax - roundedMax) < EPSILON * resolution;

        if (minSnaps && maxSnaps) {
            return i;  // Found coarsest grid that works
        }
    }

    return -1;  // Doesn't snap to any standard grid
}

BlockShape BlockShape::fromBounds(const glm::vec3& min, const glm::vec3& max) {
    // Check if it's a full cube
    constexpr float EPSILON = 0.01f;
    bool isFullCube =
        min.x < EPSILON && min.y < EPSILON && min.z < EPSILON &&
        max.x > (1.0f - EPSILON) && max.y > (1.0f - EPSILON) && max.z > (1.0f - EPSILON);

    if (isFullCube) {
        return fullCube();
    }

    // Determine appropriate voxel grid resolution PER AXIS (like Minecraft)
    // Minecraft: VoxelShapes.cuboidUnchecked()
    int bitResX = findRequiredBitResolution(min.x, max.x);
    int bitResY = findRequiredBitResolution(min.y, max.y);
    int bitResZ = findRequiredBitResolution(min.z, max.z);

    // If any axis doesn't snap to a grid, use 8×8×8 as fallback
    if (bitResX < 0 || bitResY < 0 || bitResZ < 0) {
        bitResX = bitResY = bitResZ = 3;  // 8×8×8
    }

    // Check if it's actually a full cube (all resolutions are 0 → 1×1×1)
    if (bitResX == 0 && bitResY == 0 && bitResZ == 0) {
        return fullCube();
    }

    // Calculate actual resolutions (2^i)
    int resX = 1 << bitResX;
    int resY = 1 << bitResY;
    int resZ = 1 << bitResZ;

    // Convert from float coordinates (0-1 space) to voxel indices
    int minVoxelX = static_cast<int>(std::round(min.x * resX));
    int minVoxelY = static_cast<int>(std::round(min.y * resY));
    int minVoxelZ = static_cast<int>(std::round(min.z * resZ));
    int maxVoxelX = static_cast<int>(std::round(max.x * resX));
    int maxVoxelY = static_cast<int>(std::round(max.y * resY));
    int maxVoxelZ = static_cast<int>(std::round(max.z * resZ));

    // Clamp to grid bounds
    minVoxelX = std::max(0, std::min(minVoxelX, resX));
    minVoxelY = std::max(0, std::min(minVoxelY, resY));
    minVoxelZ = std::max(0, std::min(minVoxelZ, resZ));
    maxVoxelX = std::max(0, std::min(maxVoxelX, resX));
    maxVoxelY = std::max(0, std::min(maxVoxelY, resY));
    maxVoxelZ = std::max(0, std::min(maxVoxelZ, resZ));

    // Create voxel grid with PER-AXIS resolution (like Minecraft!)
    auto voxels = BitSetVoxelSet::createBox(resX, resY, resZ,
                                            minVoxelX, minVoxelY, minVoxelZ,
                                            maxVoxelX, maxVoxelY, maxVoxelZ);

    return BlockShape(voxels);
}

bool BlockShape::isEmpty() const {
    return m_type == Type::EMPTY || !m_voxels || m_voxels->isEmpty();
}

bool BlockShape::isFullCube() const {
    return m_type == Type::FULL_CUBE;
}

std::shared_ptr<VoxelSet> BlockShape::getCullingFace(FaceDirection direction) const {
    // Fast path: empty or full cube returns self
    if (isEmpty()) {
        return nullptr;  // No geometry
    }

    if (isFullCube()) {
        return m_voxels;  // Full cube is same on all faces
    }

    // Check cache
    int dirIndex = static_cast<int>(direction);
    if (m_cullingFaces[dirIndex]) {
        return m_cullingFaces[dirIndex];
    }

    // Extract face slice (like Minecraft's SlicedVoxelShape)
    // We extract the voxel slice at the BLOCK BOUNDARY
    // Like Minecraft's getUncachedFace which uses coordinate 0.0 (NEGATIVE) or 1.0 (POSITIVE)
    //
    // For NEGATIVE direction (DOWN/WEST/NORTH): extract at index 0 (y=0.0 boundary)
    // For POSITIVE direction (UP/EAST/SOUTH): extract at last index (y=1.0 boundary)
    //
    // If the shape doesn't have voxels at that boundary (e.g., top slab's DOWN face),
    // the slice will be empty, which is correct!

    int sizeX = m_voxels->getSizeX();
    int sizeY = m_voxels->getSizeY();
    int sizeZ = m_voxels->getSizeZ();

    // Determine which slice to extract based on direction
    int sliceIndex;
    Axis axis;

    switch (direction) {
        case FaceDirection::DOWN:   // -Y face (bottom boundary at y=0)
            axis = Axis::Y;
            sliceIndex = 0;
            break;
        case FaceDirection::UP:     // +Y face (top boundary at y=1)
            axis = Axis::Y;
            sliceIndex = sizeY - 1;
            break;
        case FaceDirection::NORTH:  // -Z face (back boundary at z=0)
            axis = Axis::Z;
            sliceIndex = 0;
            break;
        case FaceDirection::SOUTH:  // +Z face (front boundary at z=1)
            axis = Axis::Z;
            sliceIndex = sizeZ - 1;
            break;
        case FaceDirection::WEST:   // -X face (left boundary at x=0)
            axis = Axis::X;
            sliceIndex = 0;
            break;
        case FaceDirection::EAST:   // +X face (right boundary at x=1)
            axis = Axis::X;
            sliceIndex = sizeX - 1;
            break;
    }

    // Create a view wrapper (no copying!) using SlicedVoxelSet
    // Like Minecraft's SlicedVoxelShape → CroppedVoxelSet
    auto faceVoxels = std::make_shared<SlicedVoxelSet>(m_voxels, axis, sliceIndex);

    // Cache the result
    m_cullingFaces[dirIndex] = faceVoxels;
    return faceVoxels;
}

glm::vec3 BlockShape::getMin() const {
    if (isEmpty()) {
        return glm::vec3(0.0f);
    }

    if (isFullCube()) {
        return glm::vec3(0.0f);
    }

    // Convert voxel coordinates to world coordinates (0-1 space)
    int sizeX = m_voxels->getSizeX();
    int sizeY = m_voxels->getSizeY();
    int sizeZ = m_voxels->getSizeZ();

    return glm::vec3(
        static_cast<float>(m_voxels->getMin(Axis::X)) / sizeX,
        static_cast<float>(m_voxels->getMin(Axis::Y)) / sizeY,
        static_cast<float>(m_voxels->getMin(Axis::Z)) / sizeZ
    );
}

glm::vec3 BlockShape::getMax() const {
    if (isEmpty()) {
        return glm::vec3(0.0f);
    }

    if (isFullCube()) {
        return glm::vec3(1.0f);
    }

    // Convert voxel coordinates to world coordinates (0-1 space)
    int sizeX = m_voxels->getSizeX();
    int sizeY = m_voxels->getSizeY();
    int sizeZ = m_voxels->getSizeZ();

    return glm::vec3(
        static_cast<float>(m_voxels->getMax(Axis::X)) / sizeX,
        static_cast<float>(m_voxels->getMax(Axis::Y)) / sizeY,
        static_cast<float>(m_voxels->getMax(Axis::Z)) / sizeZ
    );
}

} // namespace FarHorizon
