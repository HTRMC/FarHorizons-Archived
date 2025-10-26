#pragma once
#include "BlockStateNew.hpp"
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace VoxelEngine {

// Per-chunk palette that maps local indices to global blockstate IDs
// Saves memory - stores only the blockstates actually used in this chunk
class ChunkPaletteNew {
public:
    ChunkPaletteNew();

    // Get the blockstate for a local index
    uint16_t getStateId(uint8_t index) const;

    // Get the local index for a blockstate (adds to palette if not present)
    uint8_t getOrAddIndex(uint16_t stateId);

    // Get the number of entries in the palette
    size_t size() const { return m_palette.size(); }

    // Check if palette is empty (only contains AIR)
    bool isEmpty() const { return m_palette.size() <= 1; }

    // Clear the palette and reset to just AIR
    void clear();

private:
    std::vector<uint16_t> m_palette;                      // Local index -> Global state ID
    std::unordered_map<uint16_t, uint8_t> m_indexMap;     // Global state ID -> Local index
};

} // namespace VoxelEngine