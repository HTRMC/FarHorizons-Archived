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

BlockShape BlockShape::fromBounds(const glm::vec3& min, const glm::vec3& max) {
    // Check if it's a full cube
    constexpr float EPSILON = 0.01f;
    bool isFullCube =
        min.x < EPSILON && min.y < EPSILON && min.z < EPSILON &&
        max.x > (1.0f - EPSILON) && max.y > (1.0f - EPSILON) && max.z > (1.0f - EPSILON);

    if (isFullCube) {
        return fullCube();
    }

    // Determine appropriate voxel grid resolution
    // Minecraft uses power-of-2 grids: 1, 2, 4, or 8
    // We'll auto-select based on the smallest dimension
    glm::vec3 size = max - min;
    float minDim = std::min({size.x, size.y, size.z});

    int resolution;
    if (minDim >= 0.5f) {
        resolution = 2;   // Half slabs, etc.
    } else if (minDim >= 0.25f) {
        resolution = 4;   // Quarter blocks
    } else {
        resolution = 8;   // Fine details (fences, etc.)
    }

    // Create voxel grid and fill the bounded region
    // Convert from float coordinates (0-1 space) to voxel indices
    int minX = static_cast<int>(std::floor(min.x * resolution));
    int minY = static_cast<int>(std::floor(min.y * resolution));
    int minZ = static_cast<int>(std::floor(min.z * resolution));
    int maxX = static_cast<int>(std::ceil(max.x * resolution));
    int maxY = static_cast<int>(std::ceil(max.y * resolution));
    int maxZ = static_cast<int>(std::ceil(max.z * resolution));

    // Clamp to grid bounds
    minX = std::max(0, std::min(minX, resolution));
    minY = std::max(0, std::min(minY, resolution));
    minZ = std::max(0, std::min(minZ, resolution));
    maxX = std::max(0, std::min(maxX, resolution));
    maxY = std::max(0, std::min(maxY, resolution));
    maxZ = std::max(0, std::min(maxZ, resolution));

    auto voxels = BitSetVoxelSet::createBox(resolution, resolution, resolution,
                                            minX, minY, minZ, maxX, maxY, maxZ);

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
    // We need to create a new VoxelSet that only contains voxels at the face boundary

    int sizeX = m_voxels->getSizeX();
    int sizeY = m_voxels->getSizeY();
    int sizeZ = m_voxels->getSizeZ();

    // Determine which slice to extract based on direction
    int sliceIndex;
    Axis axis;

    switch (direction) {
        case FaceDirection::DOWN:   // -Y face (bottom)
            axis = Axis::Y;
            sliceIndex = 0;  // First slice along Y axis
            break;
        case FaceDirection::UP:     // +Y face (top)
            axis = Axis::Y;
            sliceIndex = sizeY - 1;  // Last slice along Y axis
            break;
        case FaceDirection::NORTH:  // -Z face (back)
            axis = Axis::Z;
            sliceIndex = 0;
            break;
        case FaceDirection::SOUTH:  // +Z face (front)
            axis = Axis::Z;
            sliceIndex = sizeZ - 1;
            break;
        case FaceDirection::WEST:   // -X face (left)
            axis = Axis::X;
            sliceIndex = 0;
            break;
        case FaceDirection::EAST:   // +X face (right)
            axis = Axis::X;
            sliceIndex = sizeX - 1;
            break;
    }

    // Create a new VoxelSet containing only the face slice
    auto faceVoxels = std::make_shared<BitSetVoxelSet>(sizeX, sizeY, sizeZ);

    // Copy voxels from the slice
    for (int x = 0; x < sizeX; x++) {
        for (int y = 0; y < sizeY; y++) {
            for (int z = 0; z < sizeZ; z++) {
                // Check if this voxel is in the slice
                bool inSlice = false;
                switch (axis) {
                    case Axis::X: inSlice = (x == sliceIndex); break;
                    case Axis::Y: inSlice = (y == sliceIndex); break;
                    case Axis::Z: inSlice = (z == sliceIndex); break;
                }

                if (inSlice && m_voxels->contains(x, y, z)) {
                    faceVoxels->set(x, y, z);
                }
            }
        }
    }

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
