#include "BlockShape.hpp"
#include "voxel/BitSetVoxelSet.hpp"
#include "voxel/CroppedVoxelSet.hpp"
#include "util/Direction.hpp"
#include <algorithm>
#include <cmath>

namespace FarHorizon {

// ============================================================================
// Static instances for common shapes
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
    : voxels_(nullptr), type_(Type::EMPTY) {
    // Empty shape has no voxels
    for (int i = 0; i < 6; i++) {
        cullingFaces_[i] = nullptr;
    }
}

BlockShape::BlockShape(std::shared_ptr<VoxelSet> voxels)
    : voxels_(voxels) {
    // Determine type
    if (!voxels || voxels->isEmpty()) {
        type_ = Type::EMPTY;
    } else if (voxels->getXSize() == 1 && voxels->getYSize() == 1 && voxels->getZSize() == 1) {
        type_ = Type::FULL_CUBE;
    } else {
        type_ = Type::PARTIAL;
    }

    // Initialize culling face cache
    for (int i = 0; i < 6; i++) {
        cullingFaces_[i] = nullptr;
    }
}

const BlockShape& BlockShape::empty() {
    return s_emptyShape;
}

const BlockShape& BlockShape::fullCube() {
    return s_fullCubeShape;
}

// Helper: Find required bit resolution for a coordinate range
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

    // Determine appropriate voxel grid resolution PER AXIS
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

    // Create voxel grid with PER-AXIS resolution (BitSetVoxelSet.java line 24)
    auto voxelSet = BitSetVoxelSet::create(resX, resY, resZ,
                                           minVoxelX, minVoxelY, minVoxelZ,
                                           maxVoxelX, maxVoxelY, maxVoxelZ);
    auto voxels = std::make_shared<BitSetVoxelSet>(voxelSet);

    return BlockShape(voxels);
}

bool BlockShape::isEmpty() const {
    return type_ == Type::EMPTY || !voxels_ || voxels_->isEmpty();
}

bool BlockShape::isFullCube() const {
    return type_ == Type::FULL_CUBE;
}

std::shared_ptr<VoxelSet> BlockShape::getCullingFace(FaceDirection direction) const {
    // Fast path: empty or full cube returns self
    if (isEmpty()) {
        return nullptr;  // No geometry
    }

    if (isFullCube()) {
        return voxels_;  // Full cube is same on all faces
    }

    // Check cache
    int dirIndex = static_cast<int>(direction);
    if (cullingFaces_[dirIndex]) {
        return cullingFaces_[dirIndex];
    }

    // Extract face slice at the block boundary
    // Uses coordinate 0.0 (NEGATIVE) or 1.0 (POSITIVE)
    //
    // For NEGATIVE direction (DOWN/WEST/NORTH): extract at index 0 (y=0.0 boundary)
    // For POSITIVE direction (UP/EAST/SOUTH): extract at last index (y=1.0 boundary)
    //
    // If the shape doesn't have voxels at that boundary (e.g., top slab's DOWN face),
    // the slice will be empty, which is correct!

    int sizeX = voxels_->getXSize();
    int sizeY = voxels_->getYSize();
    int sizeZ = voxels_->getZSize();

    // Determine which slice to extract based on direction
    int sliceIndex;
    Direction::Axis axis;

    switch (direction) {
        case FaceDirection::DOWN:   // -Y face (bottom boundary at y=0)
            axis = Direction::Axis::Y;
            sliceIndex = 0;
            break;
        case FaceDirection::UP:     // +Y face (top boundary at y=1)
            axis = Direction::Axis::Y;
            sliceIndex = sizeY - 1;
            break;
        case FaceDirection::NORTH:  // -Z face (back boundary at z=0)
            axis = Direction::Axis::Z;
            sliceIndex = 0;
            break;
        case FaceDirection::SOUTH:  // +Z face (front boundary at z=1)
            axis = Direction::Axis::Z;
            sliceIndex = sizeZ - 1;
            break;
        case FaceDirection::WEST:   // -X face (left boundary at x=0)
            axis = Direction::Axis::X;
            sliceIndex = 0;
            break;
        case FaceDirection::EAST:   // +X face (right boundary at x=1)
            axis = Direction::Axis::X;
            sliceIndex = sizeX - 1;
            break;
    }

    // Create a cropped voxel set that represents just the slice
    // CroppedVoxelSet is similar to SlicedVoxelSet but matches Minecraft's structure
    int minX = (axis == Direction::Axis::X) ? sliceIndex : 0;
    int minY = (axis == Direction::Axis::Y) ? sliceIndex : 0;
    int minZ = (axis == Direction::Axis::Z) ? sliceIndex : 0;
    int maxX = (axis == Direction::Axis::X) ? sliceIndex + 1 : sizeX;
    int maxY = (axis == Direction::Axis::Y) ? sliceIndex + 1 : sizeY;
    int maxZ = (axis == Direction::Axis::Z) ? sliceIndex + 1 : sizeZ;

    auto faceVoxels = std::make_shared<CroppedVoxelSet>(voxels_, minX, minY, minZ, maxX, maxY, maxZ);

    // Cache the result
    cullingFaces_[dirIndex] = faceVoxels;
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
    int sizeX = voxels_->getXSize();
    int sizeY = voxels_->getYSize();
    int sizeZ = voxels_->getZSize();

    return glm::vec3(
        static_cast<float>(voxels_->getMin(Direction::Axis::X)) / sizeX,
        static_cast<float>(voxels_->getMin(Direction::Axis::Y)) / sizeY,
        static_cast<float>(voxels_->getMin(Direction::Axis::Z)) / sizeZ
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
    int sizeX = voxels_->getXSize();
    int sizeY = voxels_->getYSize();
    int sizeZ = voxels_->getZSize();

    return glm::vec3(
        static_cast<float>(voxels_->getMax(Direction::Axis::X)) / sizeX,
        static_cast<float>(voxels_->getMax(Direction::Axis::Y)) / sizeY,
        static_cast<float>(voxels_->getMax(Direction::Axis::Z)) / sizeZ
    );
}

} // namespace FarHorizon
