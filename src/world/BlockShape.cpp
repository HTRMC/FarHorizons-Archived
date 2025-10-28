#include "BlockShape.hpp"

namespace VoxelEngine {

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

} // namespace VoxelEngine