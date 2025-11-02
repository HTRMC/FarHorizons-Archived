#pragma once

#include <glm/glm.hpp>
#include <optional>
#include "../world/BlockState.hpp"

namespace VoxelEngine {

class ChunkManager;

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
};

} // namespace VoxelEngine