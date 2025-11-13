#include "CollisionContext.hpp"
#include "Entity.hpp"

namespace FarHorizon {

// Static empty context instance
static EmptyCollisionContext emptyContext;

// Factory: empty context (Minecraft: static CollisionContext empty())
CollisionContext* CollisionContext::empty() {
    return &emptyContext;
}

// Factory: create from entity (Minecraft: static CollisionContext of(Entity entity))
CollisionContext* CollisionContext::of(const Entity* entity) {
    // TODO: Implement proper entity context creation
    // For now, return empty if null
    if (entity == nullptr) {
        return empty();
    }

    // TODO: Create EntityCollisionContext
    // Minecraft checks entity type (Minecart gets special handling)
    // For now, return empty as placeholder
    return empty();
}

// EmptyCollisionContext implementation
std::shared_ptr<VoxelShape> EmptyCollisionContext::getCollisionShape(const BlockState& state, CollisionGetter* collisionGetter, const glm::ivec3& pos) const {
    // TODO: Implement - should call state.getCollisionShape(collisionGetter, pos, this)
    // For now, return nullptr as placeholder
    return nullptr;
}

// EntityCollisionContext constructor
EntityCollisionContext::EntityCollisionContext(const Entity* entity)
    : entity_(entity)
{
}

// EntityCollisionContext::getCollisionShape
std::shared_ptr<VoxelShape> EntityCollisionContext::getCollisionShape(const BlockState& state, CollisionGetter* collisionGetter, const glm::ivec3& pos) const {
    // TODO: Implement - should call state.getCollisionShape(collisionGetter, pos, this)
    // For now, return nullptr as placeholder
    return nullptr;
}

// EntityCollisionContext::isDescending
bool EntityCollisionContext::isDescending() const {
    // TODO: Check if entity is descending (sneaking/crouching)
    // For now, return false
    return false;
}

// EntityCollisionContext::isAbove
bool EntityCollisionContext::isAbove(std::shared_ptr<VoxelShape> shape, const glm::ivec3& pos, bool defaultValue) const {
    // TODO: Implement - check if entity Y position is above the shape
    // For now, return defaultValue
    return defaultValue;
}

} // namespace FarHorizon