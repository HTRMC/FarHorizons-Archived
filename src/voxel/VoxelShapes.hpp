#pragma once

#include "VoxelShape.hpp"
#include "ArrayVoxelShape.hpp"
#include "BitSetVoxelSet.hpp"
#include "SlicedVoxelShape.hpp"
#include "physics/AABB.hpp"
#include "util/BooleanBiFunction.hpp"
#include <memory>
#include <vector>

namespace FarHorizon {

// Forward declarations
class BlockShape;

// Factory class for creating voxel shapes (from Minecraft's VoxelShapes.java)
class VoxelShapes {
private:
    static std::shared_ptr<VoxelShape> emptyShape;
    static std::shared_ptr<VoxelShape> fullCubeShape;

public:
    // Initialize static shapes
    static void init();

    // Get empty shape (VoxelShapes.java line 20)
    static std::shared_ptr<VoxelShape> empty();

    // Get full cube shape (VoxelShapes.java line 21)
    static std::shared_ptr<VoxelShape> fullCube();

    // Create cuboid shape (VoxelShapes.java line 56)
    static std::shared_ptr<VoxelShape> cuboid(double minX, double minY, double minZ,
                                               double maxX, double maxY, double maxZ);

    // Create VoxelShape from BlockShape at world position
    // Takes a BlockShape (in 0-1 block space) and offsets it to world coordinates
    static std::shared_ptr<VoxelShape> fromBlockShape(const BlockShape& blockShape,
                                                       int worldX, int worldY, int worldZ);

    // Collision method (Shapes.java line 184: collide)
    // This iterates through a list of voxel shapes and finds the minimum collision distance
    static double collide(Direction::Axis axis, const AABB& moving,
                         const std::vector<std::shared_ptr<VoxelShape>>& shapes,
                         double distance);

    // Check if any voxel matches the predicate (VoxelShapes.java line 156)
    // Simplified version for basic face culling - compares voxels directly
    static bool matchesAnywhere(std::shared_ptr<VoxelShape> shape1,
                                std::shared_ptr<VoxelShape> shape2,
                                const BooleanBiFunction::FunctionType& predicate);

    // Check if a face is covered by a neighbor (VoxelShapes.java line 214)
    // Used for face culling in chunk meshing
    static bool isSideCovered(std::shared_ptr<VoxelShape> shape,
                             std::shared_ptr<VoxelShape> neighbor,
                             Direction::Axis axis,
                             bool positiveDirection);

private:
    // Create a simple shape with integer bounds (VoxelShapes.java line 113)
    static std::shared_ptr<VoxelShape> create(double minX, double minY, double minZ,
                                              double maxX, double maxY, double maxZ);

    // Make index list (VoxelShapes.java line 162)
    static std::vector<double> makeIndexList(double min, double max, int size);
};

} // namespace FarHorizon