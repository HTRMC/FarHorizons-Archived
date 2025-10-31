#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <vulkan/vulkan.h>
#include <vector>

namespace VoxelEngine {

/**
 * Compact face data sent to GPU (8 bytes per face).
 * Packed bit layout for memory efficiency.
 */
struct FaceData {
    // bits 0-4: X position (0-31)
    // bits 5-9: Y position (0-31)
    // bits 10-14: Z position (0-31)
    // bits 15: isBackFace flag
    // bits 16-31: lightIndex (reference to lighting buffer)
    uint32_t packed1;

    // bits 0-15: textureIndex (texture atlas slot)
    // bits 16-31: quadIndex (reference to QuadInfo buffer)
    uint32_t packed2;

    // Helper functions for packing/unpacking
    static FaceData pack(uint32_t x, uint32_t y, uint32_t z, bool isBackFace,
                         uint32_t lightIndex, uint32_t textureIndex, uint32_t quadIndex) {
        FaceData data;
        data.packed1 = (x & 0x1F) | ((y & 0x1F) << 5) | ((z & 0x1F) << 10) |
                       ((isBackFace ? 1u : 0u) << 15) | ((lightIndex & 0xFFFF) << 16);
        data.packed2 = (textureIndex & 0xFFFF) | ((quadIndex & 0xFFFF) << 16);
        return data;
    }

    static constexpr VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(FaceData);
        binding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;  // One FaceData per face instance
        return binding;
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributes(2);

        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32_UINT;
        attributes[0].offset = offsetof(FaceData, packed1);

        attributes[1].binding = 0;
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R32_UINT;
        attributes[1].offset = offsetof(FaceData, packed2);

        return attributes;
    }
};

// Verify size
static_assert(sizeof(FaceData) == 8, "FaceData must be 8 bytes");

/**
 * Quad geometry data (shared across multiple faces).
 * Alignment matches GPU std430 layout for storage buffers.
 */
struct QuadInfo {
    // Normal vector (vec3 in shader = 12 bytes, but padded to 16 for alignment)
    alignas(16) glm::vec3 normal;
    float _padding0;  // Explicit padding to 16 bytes

    // 4 corner positions in 0-1 unit cube space
    // In std430, vec3 arrays have 16-byte stride
    alignas(16) glm::vec3 corner0;
    float _padding1;
    alignas(16) glm::vec3 corner1;
    float _padding2;
    alignas(16) glm::vec3 corner2;
    float _padding3;
    alignas(16) glm::vec3 corner3;
    float _padding4;

    // UV coordinates (vec2 = 8 bytes, array stride = 8)
    alignas(8) glm::vec2 uv0;
    alignas(8) glm::vec2 uv1;
    alignas(8) glm::vec2 uv2;
    alignas(8) glm::vec2 uv3;

    // Texture slot
    uint32_t textureSlot;
    uint32_t _padding5;  // Padding to maintain alignment
};

// Verify alignment (size will be 120 bytes with std430 layout)
static_assert(alignof(QuadInfo) == 16, "QuadInfo must be 16-byte aligned");

/**
 * Packed lighting data per face (16 bytes).
 * Each corner has 6 channels (RGB for sun, RGB for block light) packed into 5 bits each.
 */
struct PackedLighting {
    // Each uint32_t packs 6 channels of 5-bit lighting values:
    // Bits 25-29: Sun Red
    // Bits 20-24: Sun Green
    // Bits 15-19: Sun Blue
    // Bits 10-14: Block Light Red
    // Bits 5-9:   Block Light Green
    // Bits 0-4:   Block Light Blue
    uint32_t corners[4];  // One per quad corner

    // Helper to pack a single corner's lighting
    static uint32_t packCorner(uint8_t sunR, uint8_t sunG, uint8_t sunB,
                               uint8_t blockR, uint8_t blockG, uint8_t blockB) {
        return ((sunR & 0x1F) << 25) | ((sunG & 0x1F) << 20) | ((sunB & 0x1F) << 15) |
               ((blockR & 0x1F) << 10) | ((blockG & 0x1F) << 5) | (blockB & 0x1F);
    }

    // Helper to create uniform lighting (for now, until lighting system is implemented)
    static PackedLighting uniform(uint8_t sunR, uint8_t sunG, uint8_t sunB) {
        PackedLighting lighting;
        uint32_t packed = packCorner(sunR, sunG, sunB, 0, 0, 0);
        lighting.corners[0] = packed;
        lighting.corners[1] = packed;
        lighting.corners[2] = packed;
        lighting.corners[3] = packed;
        return lighting;
    }
};

static_assert(sizeof(PackedLighting) == 16, "PackedLighting must be 16 bytes");

/**
 * Mesh data for a chunk using compact format.
 */
struct CompactChunkMesh {
    std::vector<FaceData> faces;
    std::vector<PackedLighting> lighting;
    ChunkPosition position;
};

/**
 * Per-chunk data stored in SSBO (indexed by gl_BaseInstance)
 * This allows the vertex shader to transform vertices from chunk-local space to world space
 */
struct alignas(16) ChunkData {
    alignas(16) glm::ivec3 position;  // Chunk world position in blocks (chunkX * 16, chunkY * 16, chunkZ * 16)
    uint32_t faceOffset;               // Offset into FaceData buffer where this chunk's faces start

    static ChunkData create(const ChunkPosition& chunkPos, uint32_t faceBufferOffset) {
        ChunkData data;
        data.position = glm::ivec3(chunkPos.x * CHUNK_SIZE, chunkPos.y * CHUNK_SIZE, chunkPos.z * CHUNK_SIZE);
        data.faceOffset = faceBufferOffset;
        return data;
    }
};

static_assert(sizeof(ChunkData) == 16, "ChunkData must be 16 bytes");

} // namespace VoxelEngine