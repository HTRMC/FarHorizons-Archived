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
    // For partial shapes, this returns the bounding box that touches that face
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
// Uses identity-based hashing like Minecraft's VoxelShapePair
// IMPORTANT: Must include face direction because culling result depends on which face we're checking
struct ShapePair {
    const BlockShape* first;   // Our block's shape
    const BlockShape* second;  // Neighbor block's shape
    FaceDirection face;        // Which face we're checking

    bool operator==(const ShapePair& other) const {
        // Identity comparison (pointer equality) + face direction
        return first == other.first && second == other.second && face == other.face;
    }

    size_t hash() const {
        // Identity-based hash (like System.identityHashCode in Java) + face direction
        size_t h1 = first->identityHash();
        size_t h2 = second->identityHash();
        size_t h3 = static_cast<size_t>(face);
        return (h1 * 31 + h2) * 31 + h3;
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