#pragma once

#include "AABB.hpp"
#include <vector>
#include <memory>
#include <set>

namespace FarHorizon {

// Simplified VoxelShape system based on Minecraft
// Represents the collision geometry of a block
class VoxelShape {
private:
    std::vector<AABB> boxes_;
    bool isEmpty_;

public:
    VoxelShape() : isEmpty_(true) {}

    explicit VoxelShape(const std::vector<AABB>& boxes)
        : boxes_(boxes), isEmpty_(boxes.empty()) {}

    explicit VoxelShape(const AABB& box)
        : boxes_({box}), isEmpty_(false) {}

    // Factory methods for common shapes
    static VoxelShape empty() {
        return VoxelShape();
    }

    static VoxelShape fullCube() {
        static VoxelShape fullCubeShape(AABB(0, 0, 0, 1, 1, 1));
        return fullCubeShape;
    }

    static VoxelShape cube(double minX, double minY, double minZ, double maxX, double maxY, double maxZ) {
        return VoxelShape(AABB(minX, minY, minZ, maxX, maxY, maxZ));
    }

    // Getters
    bool isEmpty() const { return isEmpty_; }
    const std::vector<AABB>& getBoxes() const { return boxes_; }

    // Offset (translate) all boxes
    VoxelShape offset(double x, double y, double z) const {
        if (isEmpty_) return *this;

        std::vector<AABB> offsetBoxes;
        offsetBoxes.reserve(boxes_.size());
        for (const auto& box : boxes_) {
            offsetBoxes.push_back(box.offset(x, y, z));
        }
        return VoxelShape(offsetBoxes);
    }

    VoxelShape offset(const glm::dvec3& vec) const {
        return offset(vec.x, vec.y, vec.z);
    }

    // Calculate maximum movement distance before collision
    // This is called by the collision resolution algorithm
    double calculateMaxOffset(int axis, const AABB& entityBox, double maxDist) const {
        if (isEmpty_) return maxDist;
        if (std::abs(maxDist) < AABB::EPSILON) return 0.0;

        double result = maxDist;
        for (const auto& box : boxes_) {
            result = box.calculateMaxOffset(axis, entityBox, result);
            // Early exit if we've hit a wall
            if (std::abs(result) < AABB::EPSILON) {
                return 0.0;
            }
        }
        return result;
    }

    // Check if this shape intersects with an AABB
    bool intersects(const AABB& aabb) const {
        if (isEmpty_) return false;
        for (const auto& box : boxes_) {
            if (box.intersects(aabb)) {
                return true;
            }
        }
        return false;
    }

    // Get all unique Y-coordinates (edge positions) from the boxes
    // Used for step height calculations
    std::vector<double> getYCoordinates() const {
        std::set<double> yCoords;
        for (const auto& box : boxes_) {
            yCoords.insert(box.minY);
            yCoords.insert(box.maxY);
        }
        return std::vector<double>(yCoords.begin(), yCoords.end());
    }

    // Combine multiple VoxelShapes
    static VoxelShape combine(const std::vector<VoxelShape>& shapes) {
        std::vector<AABB> allBoxes;
        for (const auto& shape : shapes) {
            if (!shape.isEmpty()) {
                allBoxes.insert(allBoxes.end(), shape.boxes_.begin(), shape.boxes_.end());
            }
        }
        return VoxelShape(allBoxes);
    }
};

// Helper class to calculate max offset across multiple shapes
// Similar to Minecraft's VoxelShapes.calculateMaxOffset
class VoxelShapes {
public:
    static double calculateMaxOffset(int axis, const AABB& entityBox,
                                     const std::vector<VoxelShape>& shapes, double maxDist) {
        double result = maxDist;
        for (const auto& shape : shapes) {
            result = shape.calculateMaxOffset(axis, entityBox, result);
            if (std::abs(result) < AABB::EPSILON) {
                return 0.0;
            }
        }
        return result;
    }

    // Predefined shapes
    static const VoxelShape& empty() {
        static VoxelShape emptyShape = VoxelShape::empty();
        return emptyShape;
    }

    static const VoxelShape& fullCube() {
        static VoxelShape fullCubeShape = VoxelShape::fullCube();
        return fullCubeShape;
    }
};

} // namespace FarHorizon