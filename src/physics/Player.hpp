#pragma once

#include "LivingEntity.hpp"
#include <glm/glm.hpp>

namespace FarHorizon {

// Player entity with player-specific properties
// Based on Minecraft's PlayerEntity and ClientPlayerEntity classes
class Player : public LivingEntity {
public:
    // Minecraft player dimensions
    static constexpr float PLAYER_WIDTH = 0.6f;
    static constexpr float PLAYER_HEIGHT = 1.8f;
    static constexpr float PLAYER_EYE_HEIGHT = 1.62f;

    // Movement speed constants (for reference, actual speeds calculated in LivingEntity)
    static constexpr float WALK_SPEED = 4.317f;  // Blocks per second
    static constexpr float SPRINT_SPEED = 5.612f;
    static constexpr float SNEAK_SPEED = 1.295f;

public:
    Player(const glm::dvec3& position = glm::dvec3(0, 100, 0))
        : LivingEntity(EntityType::PLAYER, position, PLAYER_WIDTH, PLAYER_HEIGHT)
    {}

    // Get eye position for camera
    glm::dvec3 getEyePos() const {
        return position_ + glm::dvec3(0, PLAYER_EYE_HEIGHT, 0);
    }

    // Get interpolated eye position for smooth rendering (Minecraft's pattern)
    // partialTick: value from 0.0 to 1.0 representing progress between ticks
    // Used by renderer to provide buttery-smooth camera motion at high framerates
    glm::dvec3 getLerpedEyePos(float partialTick) const {
        // Lerp between previous and current position
        glm::dvec3 interpolatedPos = getLerpedPos(partialTick);
        return interpolatedPos + glm::dvec3(0, PLAYER_EYE_HEIGHT, 0);
    }
};

} // namespace FarHorizon