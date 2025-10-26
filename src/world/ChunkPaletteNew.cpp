#include "ChunkPaletteNew.hpp"
#include <spdlog/spdlog.h>

namespace VoxelEngine {

ChunkPaletteNew::ChunkPaletteNew() {
    // Initialize with AIR at index 0
    m_palette.push_back(0);
    m_indexMap[0] = 0;
}

uint16_t ChunkPaletteNew::getStateId(uint8_t index) const {
    if (index >= m_palette.size()) {
        spdlog::error("ChunkPaletteNew::getStateId - index {} out of bounds (size: {})",
                      index, m_palette.size());
        return 0; // Return AIR on error
    }
    return m_palette[index];
}

uint8_t ChunkPaletteNew::getOrAddIndex(uint16_t stateId) {
    // Check if state is already in palette
    auto it = m_indexMap.find(stateId);
    if (it != m_indexMap.end()) {
        return it->second;
    }

    // Add new state to palette
    if (m_palette.size() >= 256) {
        spdlog::error("ChunkPaletteNew::getOrAddIndex - palette full! Cannot add state {}", stateId);
        return 0; // Return AIR index on overflow
    }

    uint8_t newIndex = static_cast<uint8_t>(m_palette.size());
    m_palette.push_back(stateId);
    m_indexMap[stateId] = newIndex;
    return newIndex;
}

void ChunkPaletteNew::clear() {
    m_palette.clear();
    m_indexMap.clear();

    // Re-initialize with AIR
    m_palette.push_back(0);
    m_indexMap[0] = 0;
}

} // namespace VoxelEngine