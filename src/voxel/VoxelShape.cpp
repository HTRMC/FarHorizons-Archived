#include "VoxelShape.hpp"
#include "ArrayVoxelShape.hpp"
#include "OffsetDoubleList.hpp"
#include "VoxelShapes.hpp"

namespace FarHorizon {

// Move the voxel shape by an offset (VoxelShape.java line 70)
std::shared_ptr<VoxelShape> VoxelShape::move(const glm::ivec3& offset) const {
    // Fast path for empty shapes (VoxelShape.java line 71)
    if (isEmpty()) {
        return VoxelShapes::empty();
    }

    // Create offset point lists (VoxelShape.java line 71)
    // Minecraft uses OffsetDoubleList wrappers, we materialize them for simplicity
    OffsetDoubleList xOffset(getPointPositions(Direction::Axis::X), offset.x);
    OffsetDoubleList yOffset(getPointPositions(Direction::Axis::Y), offset.y);
    OffsetDoubleList zOffset(getPointPositions(Direction::Axis::Z), offset.z);

    // Materialize the offset lists
    std::vector<double> xPoints = xOffset.materialize();
    std::vector<double> yPoints = yOffset.materialize();
    std::vector<double> zPoints = zOffset.materialize();

    // Create new shape with same voxels but offset coordinates
    return std::make_shared<ArrayVoxelShape>(voxels, xPoints, yPoints, zPoints);
}

} // namespace FarHorizon