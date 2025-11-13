#pragma once

#include "../voxel/VoxelShape.hpp"
#include "AABB.hpp"
#include <glm/glm.hpp>
#include <memory>

namespace FarHorizon {

class Entity;
class CollisionGetter;
struct BlockState;

// CollisionContext interface (from Minecraft)
// Provides context for collision shape queries
class CollisionContext {
public:
    virtual ~CollisionContext() = default;

    // Factory methods (Minecraft: static CollisionContext)
    static CollisionContext* empty();
    static CollisionContext* of(const Entity* entity);

    // Get collision shape for a block state (Minecraft: VoxelShape getCollisionShape(BlockState state, CollisionGetter collisionGetter, BlockPos pos))
    virtual std::shared_ptr<VoxelShape> getCollisionShape(const BlockState& state, CollisionGetter* collisionGetter, const glm::ivec3& pos) const = 0;

    // Check if descending (Minecraft: boolean isDescending())
    virtual bool isDescending() const = 0;

    // Check if above shape (Minecraft: boolean isAbove(VoxelShape shape, BlockPos pos, boolean defaultValue))
    virtual bool isAbove(std::shared_ptr<VoxelShape> shape, const glm::ivec3& pos, bool defaultValue) const = 0;

    // Check if always collides with fluid (Minecraft: boolean alwaysCollideWithFluid())
    virtual bool alwaysCollideWithFluid() const = 0;

    // Check if placement context (Minecraft: default boolean isPlacement())
    virtual bool isPlacement() const { return false; }
};

// Empty collision context implementation
class EmptyCollisionContext : public CollisionContext {
public:
    std::shared_ptr<VoxelShape> getCollisionShape(const BlockState& state, CollisionGetter* collisionGetter, const glm::ivec3& pos) const override;
    bool isDescending() const override { return false; }
    bool isAbove(std::shared_ptr<VoxelShape> shape, const glm::ivec3& pos, bool defaultValue) const override { return defaultValue; }
    bool alwaysCollideWithFluid() const override { return false; }
};

// Entity collision context implementation
class EntityCollisionContext : public CollisionContext {
public:
    explicit EntityCollisionContext(const Entity* entity);

    std::shared_ptr<VoxelShape> getCollisionShape(const BlockState& state, CollisionGetter* collisionGetter, const glm::ivec3& pos) const override;
    bool isDescending() const override;
    bool isAbove(std::shared_ptr<VoxelShape> shape, const glm::ivec3& pos, bool defaultValue) const override;
    bool alwaysCollideWithFluid() const override { return false; }

private:
    const Entity* entity_;
};

} // namespace FarHorizon