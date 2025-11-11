#pragma once

#include "VoxelSet.hpp"
#include "FaceDirection.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <cstdint>

namespace FarHorizon {

// ============================================================================
// BlockShape - Represents the geometric shape of a block for culling
// ============================================================================
// Now uses Minecraft's VoxelShape system with 3D voxel grids
// for precise geometric culling comparisons
//
// Minecraft reference: net/minecraft/util/shape/VoxelShape.java
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

    // Check if shape is empty
    bool isEmpty() const;

    // Check if shape is a full cube (fast path optimization)
    bool isFullCube() const;

    // Get the culling face shape for a specific direction
    // Like Minecraft's VoxelShape.getFace(direction)
    // Returns a sliced VoxelSet containing only voxels at the face boundary
    std::shared_ptr<VoxelSet> getCullingFace(FaceDirection direction) const;

    // Get the underlying voxel set
    const std::shared_ptr<VoxelSet>& getVoxels() const { return m_voxels; }

    // Get bounding box (for outline rendering, etc.)
    glm::vec3 getMin() const;
    glm::vec3 getMax() const;

private:
    std::shared_ptr<VoxelSet> m_voxels;

    // Cache the type for fast path checks
    enum class Type : uint8_t {
        EMPTY,
        FULL_CUBE,
        PARTIAL
    } m_type;

    // Cached culling faces (like Minecraft's VoxelShape.shapeCache)
    mutable std::shared_ptr<VoxelSet> m_cullingFaces[6];  // One per FaceDirection
};

// ============================================================================
// Cache key for geometric face culling comparisons
// ============================================================================
// Stores two VoxelSets (face slices) for comparison
// Like Minecraft's Block.VoxelShapePair
struct ShapePair {
    std::shared_ptr<VoxelSet> first;   // Our block's face geometry
    std::shared_ptr<VoxelSet> second;  // Neighbor block's face geometry

    bool operator==(const ShapePair& other) const {
        // Use identity comparison for shared_ptr (like Minecraft)
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
