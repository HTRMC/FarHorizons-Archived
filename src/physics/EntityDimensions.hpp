#pragma once

#include "AABB.hpp"
#include <glm/glm.hpp>

namespace FarHorizon {

// EntityDimensions class (from Minecraft EntityDimensions.java)
// Stores the width and height of an entity and can create bounding boxes
class EntityDimensions {
public:
    const float width;
    const float height;
    const bool fixed;  // If true, dimensions don't scale with entity scale

    // Constructor (EntityDimensions.java constructor)
    EntityDimensions(float width, float height, bool fixed)
        : width(width), height(height), fixed(fixed) {}

    // Create bounding box from Vec3 position (EntityDimensions.java)
    AABB makeBoundingBox(const glm::dvec3& position) const {
        return makeBoundingBox(position.x, position.y, position.z);
    }

    // Create bounding box from x, y, z coordinates (EntityDimensions.java)
    // The box is centered on x/z and starts at y
    AABB makeBoundingBox(double x, double y, double z) const {
        float halfWidth = this->width / 2.0f;
        float height = this->height;
        return AABB(
            x - static_cast<double>(halfWidth),
            y,
            z - static_cast<double>(halfWidth),
            x + static_cast<double>(halfWidth),
            y + static_cast<double>(height),
            z + static_cast<double>(halfWidth)
        );
    }

    // Factory method for creating dimensions (EntityDimensions.java: static EntityDimensions scalable)
    static EntityDimensions scalable(float width, float height) {
        return EntityDimensions(width, height, false);
    }

    // Factory method for fixed dimensions (EntityDimensions.java: static EntityDimensions fixed)
    static EntityDimensions fixed(float width, float height) {
        return EntityDimensions(width, height, true);
    }
};

} // namespace FarHorizon