#pragma once

namespace VoxelEngine {

// Render type for blocks (similar to Minecraft's BlockRenderType)
enum class BlockRenderType {
    MODEL,      // Has a blockstate model file (normal blocks)
    INVISIBLE   // No rendering (air, barriers, structure voids)
};

} // namespace VoxelEngine