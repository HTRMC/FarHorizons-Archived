#pragma once

#include <cstdint>
#include <string>
#include <magic_enum/magic_enum.hpp>

namespace VoxelEngine {

// Block type enum - using uint8_t allows up to 256 different block types
enum class BlockType : uint8_t {
    AIR = 0,
    STONE = 1,
    STONE_SLAB = 2,

    // Add more block types here as needed
};

// Helper functions for block types
inline bool isBlockSolid(BlockType type) {
    return type != BlockType::AIR;
}

inline bool isBlockTransparent(BlockType type) {
    // For now, only air is transparent
    // Later we can add glass, water, etc.
    return type == BlockType::AIR;
}

inline std::string_view getBlockName(BlockType type) {
    return magic_enum::enum_name(type);
}

inline BlockType getBlockTypeFromName(std::string_view name) {
    auto type = magic_enum::enum_cast<BlockType>(name);
    return type.value_or(BlockType::AIR);
}

// Get the total number of block types
inline constexpr size_t getBlockTypeCount() {
    return magic_enum::enum_count<BlockType>();
}

} // namespace VoxelEngine