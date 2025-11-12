#pragma once

#include <glm/glm.hpp>
#include <algorithm>
#include <optional>

namespace FarHorizon {

// Axis-Aligned Bounding Box
// Based on Minecraft's Box.java class
class AABB {
public:
    // Epsilon for floating-point comparisons (from Minecraft MathHelper.approximatelyEquals)
    static constexpr double EPSILON = 1.0e-5;  // Minecraft uses 1.0E-5F

    double minX, minY, minZ;
    double maxX, maxY, maxZ;

    // Constructors
    AABB() : minX(0), minY(0), minZ(0), maxX(0), maxY(0), maxZ(0) {}

    AABB(double minX, double minY, double minZ, double maxX, double maxY, double maxZ)
        : minX(minX), minY(minY), minZ(minZ), maxX(maxX), maxY(maxY), maxZ(maxZ) {}

    AABB(const glm::dvec3& min, const glm::dvec3& max)
        : minX(min.x), minY(min.y), minZ(min.z), maxX(max.x), maxY(max.y), maxZ(max.z) {}

    // Factory methods
    static AABB fromCenter(const glm::dvec3& center, double width, double height, double depth) {
        double halfWidth = width / 2.0;
        double halfHeight = height / 2.0;
        double halfDepth = depth / 2.0;
        return AABB(
            center.x - halfWidth, center.y - halfHeight, center.z - halfDepth,
            center.x + halfWidth, center.y + halfHeight, center.z + halfDepth
        );
    }

    static AABB blockAABB(int x, int y, int z) {
        return AABB(x, y, z, x + 1.0, y + 1.0, z + 1.0);
    }

    // Getters
    glm::dvec3 getMin() const { return glm::dvec3(minX, minY, minZ); }
    glm::dvec3 getMax() const { return glm::dvec3(maxX, maxY, maxZ); }
    glm::dvec3 getCenter() const {
        return glm::dvec3(
            (minX + maxX) * 0.5,
            (minY + maxY) * 0.5,
            (minZ + maxZ) * 0.5
        );
    }

    double getWidth() const { return maxX - minX; }
    double getHeight() const { return maxY - minY; }
    double getDepth() const { return maxZ - minZ; }

    // Check if AABB intersects another AABB
    // Based on Minecraft's Box.intersects() method
    bool intersects(const AABB& other) const {
        return this->minX < other.maxX && this->maxX > other.minX &&
               this->minY < other.maxY && this->maxY > other.minY &&
               this->minZ < other.maxZ && this->maxZ > other.minZ;
    }

    // Check if AABB intersects with coordinates
    bool intersects(double minX, double minY, double minZ, double maxX, double maxY, double maxZ) const {
        return this->minX < maxX && this->maxX > minX &&
               this->minY < maxY && this->maxY > minY &&
               this->minZ < maxZ && this->maxZ > minZ;
    }

    // Check if point is contained
    bool contains(const glm::dvec3& point) const {
        return point.x >= minX && point.x <= maxX &&
               point.y >= minY && point.y <= maxY &&
               point.z >= minZ && point.z <= maxZ;
    }

    // Offset (translate) the AABB
    AABB offset(double x, double y, double z) const {
        return AABB(minX + x, minY + y, minZ + z, maxX + x, maxY + y, maxZ + z);
    }

    AABB offset(const glm::dvec3& vec) const {
        return offset(vec.x, vec.y, vec.z);
    }

    // Expand the AABB in all directions
    AABB expand(double x, double y, double z) const {
        double newMinX = minX;
        double newMinY = minY;
        double newMinZ = minZ;
        double newMaxX = maxX;
        double newMaxY = maxY;
        double newMaxZ = maxZ;

        if (x < 0.0) newMinX += x;
        else if (x > 0.0) newMaxX += x;

        if (y < 0.0) newMinY += y;
        else if (y > 0.0) newMaxY += y;

        if (z < 0.0) newMinZ += z;
        else if (z > 0.0) newMaxZ += z;

        return AABB(newMinX, newMinY, newMinZ, newMaxX, newMaxY, newMaxZ);
    }

    AABB expand(const glm::dvec3& vec) const {
        return expand(vec.x, vec.y, vec.z);
    }

    // Expand uniformly in all directions
    AABB grow(double amount) const {
        return AABB(
            minX - amount, minY - amount, minZ - amount,
            maxX + amount, maxY + amount, maxZ + amount
        );
    }

    // Shrink uniformly in all directions
    AABB shrink(double amount) const {
        return grow(-amount);
    }

    // Stretch to include a movement vector (swept volume)
    // This is crucial for Minecraft-style collision detection
    AABB stretch(const glm::dvec3& vec) const {
        double newMinX = vec.x < 0.0 ? minX + vec.x : minX;
        double newMinY = vec.y < 0.0 ? minY + vec.y : minY;
        double newMinZ = vec.z < 0.0 ? minZ + vec.z : minZ;
        double newMaxX = vec.x > 0.0 ? maxX + vec.x : maxX;
        double newMaxY = vec.y > 0.0 ? maxY + vec.y : maxY;
        double newMaxZ = vec.z > 0.0 ? maxZ + vec.z : maxZ;

        return AABB(newMinX, newMinY, newMinZ, newMaxX, newMaxY, newMaxZ);
    }

    // Get intersection with another AABB
    std::optional<AABB> intersection(const AABB& other) const {
        double newMinX = std::max(this->minX, other.minX);
        double newMinY = std::max(this->minY, other.minY);
        double newMinZ = std::max(this->minZ, other.minZ);
        double newMaxX = std::min(this->maxX, other.maxX);
        double newMaxY = std::min(this->maxY, other.maxY);
        double newMaxZ = std::min(this->maxZ, other.maxZ);

        if (newMinX >= newMaxX || newMinY >= newMaxY || newMinZ >= newMaxZ) {
            return std::nullopt;
        }

        return AABB(newMinX, newMinY, newMinZ, newMaxX, newMaxY, newMaxZ);
    }

    // Union with another AABB
    AABB unionWith(const AABB& other) const {
        return AABB(
            std::min(this->minX, other.minX),
            std::min(this->minY, other.minY),
            std::min(this->minZ, other.minZ),
            std::max(this->maxX, other.maxX),
            std::max(this->maxY, other.maxY),
            std::max(this->maxZ, other.maxZ)
        );
    }

    // Calculate maximum distance the AABB can move along an axis before colliding
    // This is the core of Minecraft's collision algorithm
    // axis: 0=X, 1=Y, 2=Z
    // maxDist: desired movement distance (can be negative)
    // Returns: actual movement distance (clamped by collision)
    double calculateMaxOffset(int axis, const AABB& other, double maxDist) const {
        // No movement requested
        if (std::abs(maxDist) < EPSILON) {
            return 0.0;
        }

        // Check if AABBs are aligned on other axes (can actually collide)
        if (axis == 0) { // X axis
            if (other.maxY <= this->minY || other.minY >= this->maxY) return maxDist;
            if (other.maxZ <= this->minZ || other.minZ >= this->maxZ) return maxDist;
        } else if (axis == 1) { // Y axis
            if (other.maxX <= this->minX || other.minX >= this->maxX) return maxDist;
            if (other.maxZ <= this->minZ || other.minZ >= this->maxZ) return maxDist;
        } else { // Z axis
            if (other.maxX <= this->minX || other.minX >= this->maxX) return maxDist;
            if (other.maxY <= this->minY || other.minY >= this->maxY) return maxDist;
        }

        double result = maxDist;

        if (maxDist > 0.0) {
            // Moving in positive direction
            // Entity's MAX (leading edge) will hit Block's MIN (near edge)
            double otherMin = axis == 0 ? other.minX : (axis == 1 ? other.minY : other.minZ);
            double thisMax = axis == 0 ? this->maxX : (axis == 1 ? this->maxY : this->maxZ);
            double gap = otherMin - thisMax;
            // CRITICAL: Only consider blocks we're approaching (gap >= 0)
            // Minecraft checks "if (f >= -1.0E-7)" at VoxelShape.java line 285
            if (gap >= -EPSILON && gap < result) {
                result = gap;
            }
        } else {
            // Moving in negative direction (falling)
            // Entity's MIN (leading edge) will hit Block's MAX (far edge)
            double otherMax = axis == 0 ? other.maxX : (axis == 1 ? other.maxY : other.maxZ);
            double thisMin = axis == 0 ? this->minX : (axis == 1 ? this->minY : this->minZ);
            double gap = otherMax - thisMin;
            // CRITICAL: Only consider blocks we're approaching (gap <= 0)
            // Minecraft checks "if (f <= 1.0E-7)" at VoxelShape.java line 300
            if (gap <= EPSILON && gap > result) {
                result = gap;
            }
        }

        return result;
    }
};

} // namespace FarHorizon