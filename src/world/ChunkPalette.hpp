#pragma once

#include "BlockState.hpp"
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace VoxelEngine {

// Per-chunk palette that maps local indices to global blockstate IDs
// This saves memory when chunks use fewer unique blockstates than the global total
class ChunkPalette {
public:
    using LocalIndex = uint16_t;  // Local index within this chunk's palette
    static constexpr LocalIndex INVALID_INDEX = 0xFFFF;
    static constexpr size_t MAX_PALETTE_SIZE = 4096;  // 2^12, fits in 12 bits

    ChunkPalette();

    // Get the blockstate ID for a local index
    BlockStateRegistry::BlockStateId getStateId(LocalIndex index) const;

    // Get the local index for a blockstate ID (adds to palette if not present)
    LocalIndex getOrAddIndex(BlockStateRegistry::BlockStateId stateId);

    // Get the local index for a blockstate ID (returns INVALID_INDEX if not in palette)
    LocalIndex getIndex(BlockStateRegistry::BlockStateId stateId) const;

    // Get the number of entries in the palette
    size_t size() const { return m_palette.size(); }

    // Check if palette is empty (only contains AIR)
    bool isEmpty() const { return m_palette.size() <= 1; }

    // Clear the palette and reset to just AIR
    void clear();

    // Get bits per block needed for current palette size
    uint8_t getBitsPerBlock() const;

private:
    std::vector<BlockStateRegistry::BlockStateId> m_palette;  // Local index -> Global ID
    std::unordered_map<BlockStateRegistry::BlockStateId, LocalIndex> m_indexMap;  // Global ID -> Local index
};

} // namespace VoxelEngine