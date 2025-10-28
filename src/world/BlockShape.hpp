#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <cstdint>

namespace VoxelEngine {

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
struct ShapePair {
    const BlockShape* first;   // Our block's shape
    const BlockShape* second;  // Neighbor block's shape

    bool operator==(const ShapePair& other) const {
        // Identity comparison (pointer equality)
        return first == other.first && second == other.second;
    }

    size_t hash() const {
        // Identity-based hash (like System.identityHashCode in Java)
        size_t h1 = first->identityHash();
        size_t h2 = second->identityHash();
        return h1 * 31 + h2;
    }
};

} // namespace VoxelEngine

// Hash specialization for ShapePair
namespace std {
    template<>
    struct hash<VoxelEngine::ShapePair> {
        size_t operator()(const VoxelEngine::ShapePair& pair) const {
            return pair.hash();
        }
    };
}