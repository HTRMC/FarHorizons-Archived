#include "Raycast.hpp"
#include "../world/ChunkManager.hpp"
#include "../world/BlockRegistry.hpp"
#include <cmath>

namespace FarHorizon {

std::optional<BlockHitResult> Raycast::castRay(
    const ChunkManager& chunkManager,
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance
) {
    // DDA algorithm (3D grid traversal)
    glm::vec3 rayDir = glm::normalize(direction);

    // Current block position
    glm::ivec3 blockPos(
        static_cast<int>(std::floor(origin.x)),
        static_cast<int>(std::floor(origin.y)),
        static_cast<int>(std::floor(origin.z))
    );

    // Step direction
    glm::ivec3 step(
        signum(rayDir.x),
        signum(rayDir.y),
        signum(rayDir.z)
    );

    // tDelta: how far along the ray we must move to cross one grid cell
    glm::vec3 tDelta(
        (rayDir.x != 0.0f) ? std::abs(1.0f / rayDir.x) : FLT_MAX,
        (rayDir.y != 0.0f) ? std::abs(1.0f / rayDir.y) : FLT_MAX,
        (rayDir.z != 0.0f) ? std::abs(1.0f / rayDir.z) : FLT_MAX
    );

    // tMax: how far along the ray we are when we cross the next grid boundary
    glm::vec3 tMax;
    if (rayDir.x > 0.0f) {
        tMax.x = (std::floor(origin.x) + 1.0f - origin.x) / rayDir.x;
    } else if (rayDir.x < 0.0f) {
        tMax.x = (std::floor(origin.x) - origin.x) / rayDir.x;
    } else {
        tMax.x = FLT_MAX;
    }

    if (rayDir.y > 0.0f) {
        tMax.y = (std::floor(origin.y) + 1.0f - origin.y) / rayDir.y;
    } else if (rayDir.y < 0.0f) {
        tMax.y = (std::floor(origin.y) - origin.y) / rayDir.y;
    } else {
        tMax.y = FLT_MAX;
    }

    if (rayDir.z > 0.0f) {
        tMax.z = (std::floor(origin.z) + 1.0f - origin.z) / rayDir.z;
    } else if (rayDir.z < 0.0f) {
        tMax.z = (std::floor(origin.z) - origin.z) / rayDir.z;
    } else {
        tMax.z = FLT_MAX;
    }

    float currentDistance = 0.0f;
    glm::ivec3 normal(0, 0, 0);

    while (currentDistance < maxDistance) {
        // Get chunk and local position
        ChunkPosition chunkPos = chunkManager.worldToChunkPos(glm::vec3(blockPos));
        const Chunk* chunk = chunkManager.getChunk(chunkPos);

        if (chunk) {
            // Calculate local block position within chunk
            glm::ivec3 localPos = blockPos - glm::ivec3(
                chunkPos.x * CHUNK_SIZE,
                chunkPos.y * CHUNK_SIZE,
                chunkPos.z * CHUNK_SIZE
            );

            // Check bounds
            if (localPos.x >= 0 && localPos.x < CHUNK_SIZE &&
                localPos.y >= 0 && localPos.y < CHUNK_SIZE &&
                localPos.z >= 0 && localPos.z < CHUNK_SIZE) {

                BlockState state = chunk->getBlockState(localPos.x, localPos.y, localPos.z);
                const Block* block = BlockRegistry::getBlock(state);

                // Check if block is solid (not air)
                if (block && block->isSolid() && block->getRenderType(state) != BlockRenderType::INVISIBLE) {
                    // Calculate exact hit position
                    glm::vec3 hitPos = origin + rayDir * currentDistance;

                    return BlockHitResult{
                        blockPos,
                        hitPos,
                        normal,
                        currentDistance,
                        state
                    };
                }
            }
        }

        // Step to next block
        if (tMax.x < tMax.y) {
            if (tMax.x < tMax.z) {
                currentDistance = tMax.x;
                tMax.x += tDelta.x;
                blockPos.x += step.x;
                normal = glm::ivec3(-step.x, 0, 0);
            } else {
                currentDistance = tMax.z;
                tMax.z += tDelta.z;
                blockPos.z += step.z;
                normal = glm::ivec3(0, 0, -step.z);
            }
        } else {
            if (tMax.y < tMax.z) {
                currentDistance = tMax.y;
                tMax.y += tDelta.y;
                blockPos.y += step.y;
                normal = glm::ivec3(0, -step.y, 0);
            } else {
                currentDistance = tMax.z;
                tMax.z += tDelta.z;
                blockPos.z += step.z;
                normal = glm::ivec3(0, 0, -step.z);
            }
        }
    }

    return std::nullopt;
}

} // namespace FarHorizon