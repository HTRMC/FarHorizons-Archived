#pragma once

#include "voxel/VoxelSet.hpp"
#include "FaceDirection.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <cstdint>
#include <functional>

namespace FarHorizon {

// ============================================================================
// BlockShape - Represents the geometric shape of a block for culling
// ============================================================================
// Uses 3D voxel grids for precise geometric culling comparisons
class BlockShape {
public:
    BlockShape(); // Empty shape
    explicit BlockShape(std::shared_ptr<VoxelSet> voxels);

    // Factory methods for common shapes
    static const BlockShape& empty();     // 0×0×0 grid (no geometry)
    static const BlockShape& fullCube();  // 1×1×1 grid (full cube)

    // Create a partial shape from bounding box
    // Resolution is auto-selected based on bounds (1×1×1, 2×2×2, 4×4×4, or 8×8×8)
    static BlockShape fromBounds(const glm::vec3& min, const glm::vec3& max);

    // Union of two shapes (voxel-level merge)
    // Returns a new shape with voxels from both shapes
    static BlockShape unionShapes(const BlockShape& shape1, const BlockShape& shape2);

    // Rotate shape using OctahedralGroup transformation
    // Returns a new shape with transformed voxel coordinates
    BlockShape rotate(const struct OctahedralGroup& rotation) const;

    // Check if shape is empty
    bool isEmpty() const;

    // Check if shape is a full cube (fast path optimization)
    bool isFullCube() const;

    // Get the culling face shape for a specific direction
    // Returns a sliced VoxelSet containing only voxels at the face boundary
    std::shared_ptr<VoxelSet> getCullingFace(FaceDirection direction) const;

    // Get the underlying voxel set
    const std::shared_ptr<VoxelSet>& getVoxels() const { return voxels_; }

    // Get bounding box (for outline rendering, etc.)
    glm::vec3 getMin() const;
    glm::vec3 getMax() const;

    // Iterate over all edges of the voxel shape (Minecraft: VoxelShape.forAllEdges)
    // Callback receives: (x1, y1, z1, x2, y2, z2) in world space [0,1]
    using EdgeConsumer = std::function<void(double, double, double, double, double, double)>;
    void forAllEdges(const EdgeConsumer& consumer) const;

private:
    std::shared_ptr<VoxelSet> voxels_;

    // Cache the type for fast path checks
    enum class Type : uint8_t {
        EMPTY,
        FULL_CUBE,
        PARTIAL
    } type_;

    // Cached culling faces
    mutable std::shared_ptr<VoxelSet> cullingFaces_[6];  // One per FaceDirection
};

// ============================================================================
// Cache key for geometric face culling comparisons
// ============================================================================
// Stores two VoxelSets (face slices) for comparison
struct ShapePair {
    std::shared_ptr<VoxelSet> first;   // Our block's face geometry
    std::shared_ptr<VoxelSet> second;  // Neighbor block's face geometry

    bool operator==(const ShapePair& other) const {
        // Use identity comparison for shared_ptr
        return first == other.first && second == other.second;
    }

    size_t hash() const {
        // Hash based on pointer addresses (identity)
        size_t h1 = std::hash<void*>{}(first.get());
        size_t h2 = std::hash<void*>{}(second.get());
        return h1 * 31 + h2;
    }
};

} // namespace FarHorizon

// Hash specialization for ShapePair
namespace std {
    template<>
    struct hash<FarHorizon::ShapePair> {
        size_t operator()(const FarHorizon::ShapePair& pair) const {
            return pair.hash();
        }
    };
}
