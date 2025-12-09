#include "Raycast.hpp"
#include "../world/ChunkManager.hpp"
#include "../world/BlockRegistry.hpp"
#include <cmath>
#include <limits>

namespace FarHorizon {

// Ray-AABB intersection using the slab method (Minecraft's AABB.clip implementation)
// Based on AABB.java getDirection() method
std::optional<float> Raycast::rayAABBIntersect(
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    const glm::vec3& boxMin,
    const glm::vec3& boxMax,
    glm::ivec3& outNormal
) {
    const float EPSILON = 1.0e-7f;

    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::max();
    glm::ivec3 normalMin(0);
    glm::ivec3 normalMax(0);

    // Test each axis (X, Y, Z)
    for (int axis = 0; axis < 3; axis++) {
        float origin = rayOrigin[axis];
        float dir = rayDir[axis];
        float min = boxMin[axis];
        float max = boxMax[axis];

        if (std::abs(dir) < EPSILON) {
            // Ray is parallel to this axis
            if (origin < min || origin > max) {
                return std::nullopt;  // Ray misses the box
            }
        } else {
            // Calculate intersection distances
            float t1 = (min - origin) / dir;
            float t2 = (max - origin) / dir;

            // Ensure t1 < t2
            if (t1 > t2) {
                std::swap(t1, t2);
            }

            // Update tMin with the face normal
            if (t1 > tMin) {
                tMin = t1;
                normalMin = glm::ivec3(0);
                normalMin[axis] = (dir > 0.0f) ? -1 : 1;
            }

            // Update tMax
            if (t2 < tMax) {
                tMax = t2;
                normalMax = glm::ivec3(0);
                normalMax[axis] = (dir > 0.0f) ? 1 : -1;
            }

            // Check for miss
            if (tMin > tMax) {
                return std::nullopt;
            }
        }
    }

    // Ray hits the box
    if (tMin >= 0.0f) {
        outNormal = normalMin;
        return tMin;
    } else if (tMax >= 0.0f) {
        // Ray starts inside the box
        outNormal = normalMax;
        return tMax;
    }

    return std::nullopt;  // Box is behind ray
}

std::optional<BlockHitResult> Raycast::castRay(
    const ChunkManager& chunkManager,
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance
) {
    // DDA algorithm (3D grid traversal) combined with shape-aware raycasting
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

    // Track closest hit across all traversed blocks
    std::optional<BlockHitResult> closestHit;
    float closestT = maxDistance;

    while (currentDistance < maxDistance) {
        // Get block state directly (ChunkManager handles chunk lookup)
        BlockState state = chunkManager.getBlockState(blockPos);

        if (!state.isAir()) {
            const Block* block = BlockRegistry::getBlock(state);

            // Check if block is solid and visible
            if (block && block->isSolid() && block->getRenderType(state) != BlockRenderType::INVISIBLE) {
                // Get the block's outline shape (Minecraft's getOutlineShape)
                BlockShape shape = block->getOutlineShape(state);

                if (!shape.isEmpty()) {
                    // Minecraft's VoxelShape.clip: test ray against ALL AABBs in the shape
                    // This correctly handles stairs, slabs, and other partial blocks

                    // Test each voxel box in the shape (Minecraft's AABB.clip(Iterable<AABB>))
                    shape.forAllBoxes([&](double minX, double minY, double minZ, double maxX, double maxY, double maxZ) {
                        // Convert from block-local [0,1] to world space
                        glm::vec3 worldMin = glm::vec3(blockPos) + glm::vec3(minX, minY, minZ);
                        glm::vec3 worldMax = glm::vec3(blockPos) + glm::vec3(maxX, maxY, maxZ);

                        // Test ray-AABB intersection
                        glm::ivec3 hitNormal;
                        std::optional<float> hitT = rayAABBIntersect(origin, rayDir, worldMin, worldMax, hitNormal);

                        if (hitT.has_value() && hitT.value() < closestT) {
                            // Found a closer hit
                            float t = hitT.value();
                            glm::vec3 hitPos = origin + rayDir * t;

                            closestHit = BlockHitResult{
                                blockPos,
                                hitPos,
                                hitNormal,
                                t,
                                state
                            };
                            closestT = t;
                        }
                    });
                }
            }
        }

        // Step to next block
        if (tMax.x < tMax.y) {
            if (tMax.x < tMax.z) {
                currentDistance = tMax.x;
                tMax.x += tDelta.x;
                blockPos.x += step.x;
            } else {
                currentDistance = tMax.z;
                tMax.z += tDelta.z;
                blockPos.z += step.z;
            }
        } else {
            if (tMax.y < tMax.z) {
                currentDistance = tMax.y;
                tMax.y += tDelta.y;
                blockPos.y += step.y;
            } else {
                currentDistance = tMax.z;
                tMax.z += tDelta.z;
                blockPos.z += step.z;
            }
        }

        // Early exit if we found a hit before reaching the next block
        if (closestHit.has_value() && closestT < currentDistance) {
            break;
        }
    }

    return closestHit;
}

} // namespace FarHorizon