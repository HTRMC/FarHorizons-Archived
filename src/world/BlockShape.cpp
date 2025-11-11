#include "BlockShape.hpp"

namespace FarHorizon {

// Static instances for common shapes (like Minecraft's VoxelShapes.empty() and VoxelShapes.fullCube())
static BlockShape s_emptyShape(ShapeType::EMPTY);
static BlockShape s_fullCubeShape(ShapeType::FULL_CUBE);

const BlockShape& BlockShape::empty() {
    return s_emptyShape;
}

const BlockShape& BlockShape::fullCube() {
    return s_fullCubeShape;
}

BlockShape BlockShape::partial(const glm::vec3& from, const glm::vec3& to) {
    BlockShape shape(ShapeType::PARTIAL);
    shape.m_min = from;
    shape.m_max = to;
    return shape;
}

FaceBounds BlockShape::getCullingFace(FaceDirection direction) const {
    // Handle empty shapes
    if (m_type == ShapeType::EMPTY) {
        return {glm::vec2(0.0f), glm::vec2(0.0f), 0.0f, true};
    }

    // Handle full cubes - return full face coverage
    if (m_type == ShapeType::FULL_CUBE) {
        // All faces cover the entire 0-1 range
        switch (direction) {
            case FaceDirection::DOWN:
                return {glm::vec2(0.0f), glm::vec2(1.0f), 0.0f, false};
            case FaceDirection::UP:
                return {glm::vec2(0.0f), glm::vec2(1.0f), 1.0f, false};
            case FaceDirection::NORTH:
                return {glm::vec2(0.0f), glm::vec2(1.0f), 0.0f, false};
            case FaceDirection::SOUTH:
                return {glm::vec2(0.0f), glm::vec2(1.0f), 1.0f, false};
            case FaceDirection::WEST:
                return {glm::vec2(0.0f), glm::vec2(1.0f), 0.0f, false};
            case FaceDirection::EAST:
                return {glm::vec2(0.0f), glm::vec2(1.0f), 1.0f, false};
        }
    }

    // Partial shapes - extract the 2D face bounds for the given direction
    FaceBounds bounds;
    bounds.isEmpty = false;

    switch (direction) {
        case FaceDirection::DOWN:  // -Y face (XZ plane)
            bounds.min = glm::vec2(m_min.x, m_min.z);
            bounds.max = glm::vec2(m_max.x, m_max.z);
            bounds.depth = m_min.y;
            break;
        case FaceDirection::UP:    // +Y face (XZ plane)
            bounds.min = glm::vec2(m_min.x, m_min.z);
            bounds.max = glm::vec2(m_max.x, m_max.z);
            bounds.depth = m_max.y;
            break;
        case FaceDirection::NORTH: // -Z face (XY plane)
            bounds.min = glm::vec2(m_min.x, m_min.y);
            bounds.max = glm::vec2(m_max.x, m_max.y);
            bounds.depth = m_min.z;
            break;
        case FaceDirection::SOUTH: // +Z face (XY plane)
            bounds.min = glm::vec2(m_min.x, m_min.y);
            bounds.max = glm::vec2(m_max.x, m_max.y);
            bounds.depth = m_max.z;
            break;
        case FaceDirection::WEST:  // -X face (YZ plane)
            bounds.min = glm::vec2(m_min.y, m_min.z);
            bounds.max = glm::vec2(m_max.y, m_max.z);
            bounds.depth = m_min.x;
            break;
        case FaceDirection::EAST:  // +X face (YZ plane)
            bounds.min = glm::vec2(m_min.y, m_min.z);
            bounds.max = glm::vec2(m_max.y, m_max.z);
            bounds.depth = m_max.x;
            break;
    }

    return bounds;
}

} // namespace FarHorizon