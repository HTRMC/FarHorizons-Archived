#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <cstdint>
#include "FaceDirection.hpp"

namespace FarHorizon {

// Represents the geometric shape of a block for culling purposes
// Inspired by Minecraft's VoxelShape system but simplified
enum class ShapeType : uint8_t {
    EMPTY,      // No geometry (air, cave_air, etc.)
    FULL_CUBE,  // Complete 1x1x1 cube (stone, dirt, etc.)
    PARTIAL     // Custom geometry (slabs, stairs, etc.)
};

// Represents the 2D bounds of a face for culling comparisons
// Like Minecraft's VoxelShape but extracted for a specific face
struct FaceBounds {
    glm::vec2 min;  // Minimum UV coordinates (0-1 space)
    glm::vec2 max;  // Maximum UV coordinates (0-1 space)
    float depth;    // Position along the face's axis (0-1 space)
    bool isEmpty;   // True if no geometry exists on this face

    bool operator==(const FaceBounds& other) const {
        if (isEmpty && other.isEmpty) return true;
        if (isEmpty != other.isEmpty) return false;
        constexpr float EPSILON = 1e-6f;
        return std::abs(min.x - other.min.x) < EPSILON &&
               std::abs(min.y - other.min.y) < EPSILON &&
               std::abs(max.x - other.max.x) < EPSILON &&
               std::abs(max.y - other.max.y) < EPSILON &&
               std::abs(depth - other.depth) < EPSILON;
    }

    size_t hash() const {
        if (isEmpty) return 0;
        // Simple hash combining min, max, and depth
        size_t h = 0;
        h ^= std::hash<float>{}(min.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<float>{}(min.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<float>{}(max.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<float>{}(max.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<float>{}(depth) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

// Represents a block's culling shape
// Used to determine if adjacent faces should be rendered
class BlockShape {
public:
    BlockShape() : m_type(ShapeType::EMPTY) {}  // Default constructor for std::unordered_map
    BlockShape(ShapeType type) : m_type(type) {}

    // Factory methods for common shapes
    static const BlockShape& empty();
    static const BlockShape& fullCube();
    static BlockShape partial(const glm::vec3& from, const glm::vec3& to);

    ShapeType getType() const { return m_type; }

    // Get the culling face shape for a specific direction
    // Like Minecraft's BlockState.getCullingFace(direction)
    // Extracts the 2D face geometry that's relevant for culling
    FaceBounds getCullingFace(FaceDirection direction) const;

    // For partial shapes, get the full bounding box
    const glm::vec3& getMin() const { return m_min; }
    const glm::vec3& getMax() const { return m_max; }

    // Identity-based comparison for cache keys (like Minecraft's VoxelShapePair)
    bool identityEquals(const BlockShape& other) const {
        return this == &other;
    }

    size_t identityHash() const {
        return reinterpret_cast<size_t>(this);
    }

private:
    ShapeType m_type;
    glm::vec3 m_min{0.0f};  // Bounding box min (0-1 space)
    glm::vec3 m_max{1.0f};  // Bounding box max (0-1 space)
};

// Cache key for geometric face culling comparisons
// Like Minecraft's VoxelShapePair - stores extracted face geometries
// NOTE: No direction needed! Direction is handled by extracting face-specific geometry beforehand
struct ShapePair {
    FaceBounds first;   // Our block's face geometry
    FaceBounds second;  // Neighbor block's face geometry

    bool operator==(const ShapePair& other) const {
        return first == other.first && second == other.second;
    }

    size_t hash() const {
        // Combine hashes of both FaceBounds
        return first.hash() * 31 + second.hash();
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