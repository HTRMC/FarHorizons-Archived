#pragma once

#include <glm/glm.hpp>
#include <optional>
#include "../world/BlockState.hpp"
#include "../world/ChunkManager.hpp"
#include "../world/BlockShape.hpp"

namespace FarHorizon {

struct BlockHitResult {
    glm::ivec3 blockPos;
    glm::vec3 hitPos;
    glm::ivec3 normal;
    float distance;
    BlockState state;
};

class Raycast {
public:
    static std::optional<BlockHitResult> castRay(
        const ChunkManager& chunkManager,
        const glm::vec3& origin,
        const glm::vec3& direction,
        float maxDistance = 8.0f
    );

private:
    static int signum(float val) {
        return (0.0f < val) - (val < 0.0f);
    }

    // Ray-AABB intersection test (Minecraft's AABB.clip)
    // Returns hit distance (t value along ray) or nullopt if no hit
    // Also outputs the hit normal
    static std::optional<float> rayAABBIntersect(
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDir,
        const glm::vec3& boxMin,
        const glm::vec3& boxMax,
        glm::ivec3& outNormal
    );
};

} // namespace FarHorizon