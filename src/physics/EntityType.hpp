#pragma once

namespace FarHorizon {

// EntityType enum (from Minecraft EntityType.java)
// Represents the type of entity (player, zombie, arrow, etc.)
// In Minecraft, this is a registry with properties like dimensions, fire immunity, etc.
enum class EntityType {
    PLAYER,
    LIVING_ENTITY,
    ZOMBIE,
    SKELETON,
    CREEPER,
    SPIDER,
    ARROW,
    ITEM,
    // Add more entity types as needed
};

} // namespace FarHorizon