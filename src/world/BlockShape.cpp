#include "BlockShape.hpp"
#include "voxel/BitSetVoxelSet.hpp"
#include "voxel/CroppedVoxelSet.hpp"
#include "util/Direction.hpp"
#include "util/OctahedralGroup.hpp"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

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

        spdlog::info("BlockShape static init: voxels size={}x{}x{} maxX={} maxY={} maxZ={} isFullCube={}",
            s_fullCubeShape.getVoxels()->getXSize(),
            s_fullCubeShape.getVoxels()->getYSize(),
            s_fullCubeShape.getVoxels()->getZSize(),
            s_fullCubeShape.getVoxels()->getMax(Direction::Axis::X),
            s_fullCubeShape.getVoxels()->getMax(Direction::Axis::Y),
            s_fullCubeShape.getVoxels()->getMax(Direction::Axis::Z),
            s_fullCubeShape.isFullCube());
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

BlockShape BlockShape::unionShapes(const BlockShape& shape1, const BlockShape& shape2) {
    spdlog::info("unionShapes: shape1 isEmpty={}, shape2 isEmpty={}", shape1.isEmpty(), shape2.isEmpty());

    // Handle empty cases
    if (shape1.isEmpty()) {
        return shape2;
    }
    if (shape2.isEmpty()) {
        return shape1;
    }

    // Get bounding boxes of both shapes
    glm::vec3 min1 = shape1.getMin();
    glm::vec3 max1 = shape1.getMax();
    glm::vec3 min2 = shape2.getMin();
    glm::vec3 max2 = shape2.getMax();

    spdlog::info("  shape1: bounds=({},{},{}) to ({},{},{}), voxelSize={}x{}x{}",
                 min1.x, min1.y, min1.z, max1.x, max1.y, max1.z,
                 shape1.voxels_->getXSize(), shape1.voxels_->getYSize(), shape1.voxels_->getZSize());
    spdlog::info("  shape2: bounds=({},{},{}) to ({},{},{}), voxelSize={}x{}x{}",
                 min2.x, min2.y, min2.z, max2.x, max2.y, max2.z,
                 shape2.voxels_->getXSize(), shape2.voxels_->getYSize(), shape2.voxels_->getZSize());

    // Calculate merged bounding box
    glm::vec3 mergedMin = glm::min(min1, min2);
    glm::vec3 mergedMax = glm::max(max1, max2);

    // Determine resolution for the merged shape
    // Use the maximum resolution from both shapes to preserve detail
    int resX = std::max(shape1.voxels_->getXSize(), shape2.voxels_->getXSize());
    int resY = std::max(shape1.voxels_->getYSize(), shape2.voxels_->getYSize());
    int resZ = std::max(shape1.voxels_->getZSize(), shape2.voxels_->getZSize());

    // Ensure resolution is power of 2 and at most 8
    resX = std::min(8, std::max(1, resX));
    resY = std::min(8, std::max(1, resY));
    resZ = std::min(8, std::max(1, resZ));

    spdlog::info("  merged resolution: {}x{}x{}", resX, resY, resZ);

    // Convert merged bounds to voxel indices
    int minX = static_cast<int>(std::floor(mergedMin.x * resX));
    int minY = static_cast<int>(std::floor(mergedMin.y * resY));
    int minZ = static_cast<int>(std::floor(mergedMin.z * resZ));
    int maxX = static_cast<int>(std::ceil(mergedMax.x * resX));
    int maxY = static_cast<int>(std::ceil(mergedMax.y * resY));
    int maxZ = static_cast<int>(std::ceil(mergedMax.z * resZ));

    // Clamp to grid bounds
    minX = std::max(0, std::min(minX, resX));
    minY = std::max(0, std::min(minY, resY));
    minZ = std::max(0, std::min(minZ, resZ));
    maxX = std::max(0, std::min(maxX, resX));
    maxY = std::max(0, std::min(maxY, resY));
    maxZ = std::max(0, std::min(maxZ, resZ));

    // Create merged voxel set
    auto mergedVoxels = std::make_shared<BitSetVoxelSet>(resX, resY, resZ);

    // Fill voxels from shape1
    for (int x = 0; x < resX; x++) {
        for (int y = 0; y < resY; y++) {
            for (int z = 0; z < resZ; z++) {
                // Map to shape1's coordinate space
                float worldX = (x + 0.5f) / resX;
                float worldY = (y + 0.5f) / resY;
                float worldZ = (z + 0.5f) / resZ;

                // Map to shape1's voxel space (world coords [0,1] -> voxel indices [0,size])
                int x1 = static_cast<int>(worldX * shape1.voxels_->getXSize());
                int y1 = static_cast<int>(worldY * shape1.voxels_->getYSize());
                int z1 = static_cast<int>(worldZ * shape1.voxels_->getZSize());

                // Clamp to valid voxel indices
                x1 = std::max(0, std::min(x1, shape1.voxels_->getXSize() - 1));
                y1 = std::max(0, std::min(y1, shape1.voxels_->getYSize() - 1));
                z1 = std::max(0, std::min(z1, shape1.voxels_->getZSize() - 1));

                // Check if this voxel is filled in shape1
                if (shape1.voxels_->contains(x1, y1, z1)) {
                    mergedVoxels->set(x, y, z);
                    continue;
                }

                // Map to shape2's voxel space (world coords [0,1] -> voxel indices [0,size])
                int x2 = static_cast<int>(worldX * shape2.voxels_->getXSize());
                int y2 = static_cast<int>(worldY * shape2.voxels_->getYSize());
                int z2 = static_cast<int>(worldZ * shape2.voxels_->getZSize());

                // Clamp to valid voxel indices
                x2 = std::max(0, std::min(x2, shape2.voxels_->getXSize() - 1));
                y2 = std::max(0, std::min(y2, shape2.voxels_->getYSize() - 1));
                z2 = std::max(0, std::min(z2, shape2.voxels_->getZSize() - 1));

                // Check if this voxel is filled in shape2
                if (shape2.voxels_->contains(x2, y2, z2)) {
                    mergedVoxels->set(x, y, z);
                }
            }
        }
    }

    return BlockShape(mergedVoxels);
}

BlockShape BlockShape::rotate(const OctahedralGroup& rotation) const {
    // Handle empty shapes
    if (isEmpty()) {
        return empty();
    }

    // Full cubes remain full cubes after rotation
    if (isFullCube()) {
        return fullCube();
    }

    // Get source dimensions
    int sizeX = voxels_->getXSize();
    int sizeY = voxels_->getYSize();
    int sizeZ = voxels_->getZSize();

    // Create destination voxel grid with same dimensions
    auto rotatedVoxels = std::make_shared<BitSetVoxelSet>(sizeX, sizeY, sizeZ);

    // Transform each filled voxel
    for (int x = 0; x < sizeX; x++) {
        for (int y = 0; y < sizeY; y++) {
            for (int z = 0; z < sizeZ; z++) {
                if (!voxels_->contains(x, y, z)) {
                    continue;  // Skip empty voxels
                }

                // Convert voxel center to normalized 0-1 space
                glm::vec3 center(
                    (x + 0.5f) / sizeX,
                    (y + 0.5f) / sizeY,
                    (z + 0.5f) / sizeZ
                );

                // Apply transformation
                glm::vec3 transformed = rotation.transform(center);

                // Convert back to voxel coordinates
                int newX = static_cast<int>(std::floor(transformed.x * sizeX));
                int newY = static_cast<int>(std::floor(transformed.y * sizeY));
                int newZ = static_cast<int>(std::floor(transformed.z * sizeZ));

                // Clamp to grid bounds (in case of floating point errors)
                newX = std::max(0, std::min(newX, sizeX - 1));
                newY = std::max(0, std::min(newY, sizeY - 1));
                newZ = std::max(0, std::min(newZ, sizeZ - 1));

                // Set the voxel in the rotated grid
                rotatedVoxels->set(newX, newY, newZ);
            }
        }
    }

    return BlockShape(rotatedVoxels);
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
