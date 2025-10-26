#include "ChunkPalette.hpp"
#include <stdexcept>
#include <cmath>

namespace VoxelEngine {

ChunkPalette::ChunkPalette() {
    // Initialize with AIR at index 0
    m_palette.reserve(16);  // Reserve space for common case
    m_palette.push_back(BlockStateRegistry::AIR_ID);
    m_indexMap[BlockStateRegistry::AIR_ID] = 0;
}

BlockStateRegistry::BlockStateId ChunkPalette::getStateId(LocalIndex index) const {
    if (index >= m_palette.size()) {
        return BlockStateRegistry::AIR_ID;
    }
    return m_palette[index];
}

ChunkPalette::LocalIndex ChunkPalette::getOrAddIndex(BlockStateRegistry::BlockStateId stateId) {
    // Check if already in palette
    auto it = m_indexMap.find(stateId);
    if (it != m_indexMap.end()) {
        return it->second;
    }

    // Add to palette
    if (m_palette.size() >= MAX_PALETTE_SIZE) {
        throw std::runtime_error("Chunk palette overflow: too many unique blockstates in chunk");
    }

    LocalIndex newIndex = static_cast<LocalIndex>(m_palette.size());
    m_palette.push_back(stateId);
    m_indexMap[stateId] = newIndex;

    return newIndex;
}

ChunkPalette::LocalIndex ChunkPalette::getIndex(BlockStateRegistry::BlockStateId stateId) const {
    auto it = m_indexMap.find(stateId);
    if (it != m_indexMap.end()) {
        return it->second;
    }
    return INVALID_INDEX;
}

void ChunkPalette::clear() {
    m_palette.clear();
    m_indexMap.clear();

    // Re-initialize with AIR
    m_palette.push_back(BlockStateRegistry::AIR_ID);
    m_indexMap[BlockStateRegistry::AIR_ID] = 0;
}

uint8_t ChunkPalette::getBitsPerBlock() const {
    if (m_palette.size() <= 1) {
        return 1;  // Only AIR
    } else if (m_palette.size() <= 16) {
        return 4;  // 4 bits = 16 states
    } else if (m_palette.size() <= 256) {
        return 8;  // 8 bits = 256 states
    } else {
        return 12;  // 12 bits = 4096 states
    }
}

} // namespace VoxelEngine