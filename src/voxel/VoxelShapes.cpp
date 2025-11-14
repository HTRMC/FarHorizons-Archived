#include "VoxelShapes.hpp"
#include "world/BlockShape.hpp"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

namespace FarHorizon {

// Static shape instances
std::shared_ptr<VoxelShape> VoxelShapes::emptyShape;
std::shared_ptr<VoxelShape> VoxelShapes::fullCubeShape;

// Initialize static shapes (VoxelShapes.java static initializer)
void VoxelShapes::init() {
    if (!emptyShape) {
        // Empty shape: 0x0x0 voxel grid
        auto emptyVoxels = std::make_shared<BitSetVoxelSet>(0, 0, 0);
        std::vector<double> emptyPoints = {0.0};
        emptyShape = std::make_shared<ArrayVoxelShape>(emptyVoxels, emptyPoints, emptyPoints, emptyPoints);
    }

    if (!fullCubeShape) {
        // Full cube: 1x1x1 voxel grid from 0 to 1
        auto cubeVoxels = std::make_shared<BitSetVoxelSet>(1, 1, 1);
        cubeVoxels->set(0, 0, 0);
        std::vector<double> cubePoints = {0.0, 1.0};
        fullCubeShape = std::make_shared<ArrayVoxelShape>(cubeVoxels, cubePoints, cubePoints, cubePoints);
    }
}

// Get empty shape (VoxelShapes.java line 20)
std::shared_ptr<VoxelShape> VoxelShapes::empty() {
    if (!emptyShape) init();
    return emptyShape;
}

// Get full cube shape (VoxelShapes.java line 21)
std::shared_ptr<VoxelShape> VoxelShapes::fullCube() {
    if (!fullCubeShape) init();
    return fullCubeShape;
}

// Create cuboid shape (VoxelShapes.java line 56)
std::shared_ptr<VoxelShape> VoxelShapes::cuboid(double minX, double minY, double minZ,
                                                 double maxX, double maxY, double maxZ) {
    // Handle empty case (VoxelShapes.java line 57)
    if (minX >= maxX || minY >= maxY || minZ >= maxZ) {
        return empty();
    }

    return create(minX, minY, minZ, maxX, maxY, maxZ);
}

// Create VoxelShape from BlockShape at world position
std::shared_ptr<VoxelShape> VoxelShapes::fromBlockShape(const BlockShape& blockShape,
                                                         int worldX, int worldY, int worldZ) {
    // Fast path: empty shape
    if (blockShape.isEmpty()) {
        return empty();
    }

    // Fast path: full cube
    if (blockShape.isFullCube()) {
        // Create full cube in block-local [0, 1] space, then move to world position
        return fullCube()->move(worldX, worldY, worldZ);
    }

    // Partial shape: use bounding box approach
    // Get min/max bounds in block-local [0, 1] space from BlockShape
    glm::vec3 minLocal = blockShape.getMin();
    glm::vec3 maxLocal = blockShape.getMax();

    // Create shape in block-local coordinates [0, 1]
    auto localShape = cuboid(minLocal.x, minLocal.y, minLocal.z,
                             maxLocal.x, maxLocal.y, maxLocal.z);

    // Move to world position (Minecraft: shape.move(blockPos))
    return localShape->move(worldX, worldY, worldZ);
}

// Create a shape with specified bounds (VoxelShapes.java line 113)
std::shared_ptr<VoxelShape> VoxelShapes::create(double minX, double minY, double minZ,
                                                 double maxX, double maxY, double maxZ) {
    // Calculate voxel grid size (VoxelShapes.java line 114-116)
    int sizeX = static_cast<int>(std::ceil(maxX - minX));
    int sizeY = static_cast<int>(std::ceil(maxY - minY));
    int sizeZ = static_cast<int>(std::ceil(maxZ - minZ));

    // Clamp to reasonable size
    sizeX = std::max(1, std::min(sizeX, 16));
    sizeY = std::max(1, std::min(sizeY, 16));
    sizeZ = std::max(1, std::min(sizeZ, 16));

    // Calculate voxel indices (VoxelShapes.java line 119-124)
    int minVoxelX = static_cast<int>(std::floor(minX));
    int minVoxelY = static_cast<int>(std::floor(minY));
    int minVoxelZ = static_cast<int>(std::floor(minZ));
    int maxVoxelX = static_cast<int>(std::ceil(maxX));
    int maxVoxelY = static_cast<int>(std::ceil(maxY));
    int maxVoxelZ = static_cast<int>(std::ceil(maxZ));

    // Create voxel set (VoxelShapes.java line 125)
    auto voxels = std::make_shared<BitSetVoxelSet>(sizeX, sizeY, sizeZ);

    // Fill the voxels in the specified region (VoxelShapes.java line 126-132)
    for (int x = minVoxelX; x < maxVoxelX; x++) {
        for (int y = minVoxelY; y < maxVoxelY; y++) {
            for (int z = minVoxelZ; z < maxVoxelZ; z++) {
                int voxelX = x - minVoxelX;
                int voxelY = y - minVoxelY;
                int voxelZ = z - minVoxelZ;
                if (voxelX >= 0 && voxelX < sizeX &&
                    voxelY >= 0 && voxelY < sizeY &&
                    voxelZ >= 0 && voxelZ < sizeZ) {
                    voxels->set(voxelX, voxelY, voxelZ);
                }
            }
        }
    }

    // Create point lists (VoxelShapes.java line 133-135)
    std::vector<double> xPoints = makeIndexList(minX, maxX, sizeX);
    std::vector<double> yPoints = makeIndexList(minY, maxY, sizeY);
    std::vector<double> zPoints = makeIndexList(minZ, maxZ, sizeZ);

    return std::make_shared<ArrayVoxelShape>(voxels, xPoints, yPoints, zPoints);
}

// Make index list for shape points (VoxelShapes.java line 162)
std::vector<double> VoxelShapes::makeIndexList(double min, double max, int size) {
    std::vector<double> points;
    points.reserve(size + 1);

    // Add start point
    points.push_back(min);

    // Add intermediate points
    for (int i = 1; i < size; i++) {
        double t = static_cast<double>(i) / static_cast<double>(size);
        points.push_back(min + t * (max - min));
    }

    // Add end point
    points.push_back(max);

    return points;
}

// Collision method (Shapes.java line 184: collide)
double VoxelShapes::collide(Direction::Axis axis, const AABB& moving,
                           const std::vector<std::shared_ptr<VoxelShape>>& shapes,
                           double distance) {
    // Early exit for no shapes
    if (shapes.empty()) {
        return distance;
    }

    // Early exit for zero movement
    if (std::abs(distance) < 1.0E-7) {
        return 0.0;
    }

    double result = distance;

    // Debug logging - only when there are shapes and we're moving on Y axis
    bool shouldDebug = !shapes.empty() && axis == Direction::Axis::Y;
    if (shouldDebug) {
        spdlog::info("Shapes.collide: axis=Y moving=({:.3f},{:.3f},{:.3f})-({:.3f},{:.3f},{:.3f}) distance={:.6f} shapes={}",
            moving.minX, moving.minY, moving.minZ, moving.maxX, moving.maxY, moving.maxZ, distance, shapes.size());
    }

    // Iterate through all shapes and find the minimum collision distance
    int shapeIndex = 0;
    for (const auto& shape : shapes) {
        if (shape && !shape->isEmpty()) {
            double oldResult = result;
            result = shape->collide(axis, moving, result);

            if (shouldDebug && shapeIndex < 3) {
                spdlog::info("  Shape {}: oldResult={:.6f} newResult={:.6f}", shapeIndex, oldResult, result);
            }

            // Early exit if we've hit a collision
            if (std::abs(result) < 1.0E-7) {
                if (shouldDebug) {
                    spdlog::info("  Early exit: collision detected!");
                }
                return 0.0;
            }
        }
        shapeIndex++;
    }

    if (shouldDebug) {
        spdlog::info("  Final result: {:.6f}", result);
    }

    return result;
}

// Check if any voxel matches the predicate (VoxelShapes.java line 156)
// Simplified version that directly compares voxels
bool VoxelShapes::matchesAnywhere(std::shared_ptr<VoxelShape> shape1,
                                  std::shared_ptr<VoxelShape> shape2,
                                  const BooleanBiFunction::FunctionType& predicate) {
    // Early exit checks (VoxelShapes.java line 157-161)
    if (predicate(false, false)) {
        throw std::invalid_argument("Predicate should not apply to (false, false)");
    }

    bool empty1 = shape1->isEmpty();
    bool empty2 = shape2->isEmpty();

    // If both empty or one empty (VoxelShapes.java line 162-163)
    if (empty1 || empty2) {
        return predicate(!empty1, !empty2);
    }

    // Same shape optimization (VoxelShapes.java line 163-164)
    if (shape1 == shape2) {
        return predicate(true, true);
    }

    // Simplified direct voxel comparison
    // In full Minecraft, this uses PairList for efficient iteration
    // For now, we do a direct comparison of all voxels
    const VoxelSet& voxels1 = shape1->getVoxels();
    const VoxelSet& voxels2 = shape2->getVoxels();

    // Get bounds
    int maxX = std::max(voxels1.getXSize(), voxels2.getXSize());
    int maxY = std::max(voxels1.getYSize(), voxels2.getYSize());
    int maxZ = std::max(voxels1.getZSize(), voxels2.getZSize());

    // Compare all voxel positions
    for (int x = 0; x < maxX; x++) {
        for (int y = 0; y < maxY; y++) {
            for (int z = 0; z < maxZ; z++) {
                bool in1 = voxels1.inBoundsAndContains(x, y, z);
                bool in2 = voxels2.inBoundsAndContains(x, y, z);
                if (predicate(in1, in2)) {
                    return true;
                }
            }
        }
    }

    return false;
}

// Check if a face is covered by a neighbor (VoxelShapes.java line 214)
bool VoxelShapes::isSideCovered(std::shared_ptr<VoxelShape> shape,
                               std::shared_ptr<VoxelShape> neighbor,
                               Direction::Axis axis,
                               bool positiveDirection) {
    // Fast path: both full cubes (VoxelShapes.java line 215-216)
    if (shape == fullCube() && neighbor == fullCube()) {
        return true;
    }

    // If neighbor is empty, face is not covered (VoxelShapes.java line 217-218)
    if (neighbor->isEmpty()) {
        return false;
    }

    // Determine which shape to slice (VoxelShapes.java line 222-223)
    auto voxelShape = positiveDirection ? shape : neighbor;
    auto voxelShape2 = positiveDirection ? neighbor : shape;

    // Select the predicate (VoxelShapes.java line 224)
    auto booleanBiFunction = positiveDirection ? BooleanBiFunction::ONLY_FIRST : BooleanBiFunction::ONLY_SECOND;

    // Check if shapes meet at the boundary (VoxelShapes.java line 225-226)
    const double EPSILON = 1.0E-7;
    double max1 = voxelShape->getPointPositions(axis).back();
    double min2 = voxelShape2->getPointPositions(axis).front();

    if (std::abs(max1 - 1.0) > EPSILON || std::abs(min2 - 0.0) > EPSILON) {
        return false;
    }

    // Create sliced shapes for the touching faces (VoxelShapes.java line 227-228)
    int sliceIndex1 = voxelShape->getVoxels().getSize(axis) - 1;
    auto sliced1 = std::make_shared<SlicedVoxelShape>(voxelShape, axis, sliceIndex1);
    auto sliced2 = std::make_shared<SlicedVoxelShape>(voxelShape2, axis, 0);

    // Check if the sliced shapes have any mismatched voxels (VoxelShapes.java line 227-229)
    return !matchesAnywhere(sliced1, sliced2, booleanBiFunction);
}

} // namespace FarHorizon