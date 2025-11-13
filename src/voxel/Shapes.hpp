#pragma once

#include "VoxelShape.hpp"
#include "BooleanOp.hpp"
#include "../physics/AABB.hpp"
#include <memory>

namespace FarHorizon {

// Shapes utility class (from Minecraft)
// Contains static factory methods and operations for VoxelShapes
class Shapes {
public:
    // Constants (Minecraft: public static final double)
    static constexpr double EPSILON = 1.0E-7;
    static constexpr double BIG_EPSILON = 1.0E-6;

    // Get empty shape (Minecraft: public static VoxelShape empty())
    static std::shared_ptr<VoxelShape> empty();

    // Get full block shape (Minecraft: public static VoxelShape block())
    static std::shared_ptr<VoxelShape> block();

    // Create VoxelShape from AABB (Minecraft: public static VoxelShape create(AABB aabb))
    static std::shared_ptr<VoxelShape> create(const AABB& aabb);

    // Create VoxelShape from coordinates (Minecraft: public static VoxelShape create(double minX, minY, minZ, maxX, maxY, maxZ))
    static std::shared_ptr<VoxelShape> create(double minX, double minY, double minZ, double maxX, double maxY, double maxZ);

    // Check if boolean join results in non-empty shape (Minecraft: public static boolean joinIsNotEmpty(VoxelShape first, VoxelShape second, BooleanOp op))
    static bool joinIsNotEmpty(std::shared_ptr<VoxelShape> first, std::shared_ptr<VoxelShape> second, BooleanOp op);

private:
    // Private constructor - utility class
    Shapes() = delete;

    // Cached shapes
    static std::shared_ptr<VoxelShape> BLOCK_;
    static std::shared_ptr<VoxelShape> EMPTY_;
};

} // namespace FarHorizon