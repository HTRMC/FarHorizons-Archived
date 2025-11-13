#include "ChunkPalette.hpp"
#include <spdlog/spdlog.h>

namespace FarHorizon {

ChunkPalette::ChunkPalette() {
    // Initialize with AIR at index 0
    palette_.push_back(0);
    indexMap_[0] = 0;
}

uint16_t ChunkPalette::getStateId(uint8_t index) const {
    if (index >= palette_.size()) {
        spdlog::error("ChunkPaletteNew::getStateId - index {} out of bounds (size: {})",
                      index, palette_.size());
        return 0; // Return AIR on error
    }
    return palette_[index];
}

uint8_t ChunkPalette::getOrAddIndex(uint16_t stateId) {
    // Check if state is already in palette
    auto it = indexMap_.find(stateId);
    if (it != indexMap_.end()) {
        return it->second;
    }

    // Add new state to palette
    if (palette_.size() >= 256) {
        spdlog::error("ChunkPaletteNew::getOrAddIndex - palette full! Cannot add state {}", stateId);
        return 0; // Return AIR index on overflow
    }

    uint8_t newIndex = static_cast<uint8_t>(palette_.size());
    palette_.push_back(stateId);
    indexMap_[stateId] = newIndex;
    return newIndex;
}

void ChunkPalette::clear() {
    palette_.clear();
    indexMap_.clear();

    // Re-initialize with AIR
    palette_.push_back(0);
    indexMap_[0] = 0;
}

} // namespace FarHorizon