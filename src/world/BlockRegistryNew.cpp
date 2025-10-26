#include "BlockRegistryNew.hpp"
#include "BlockModel.hpp"
#include <spdlog/spdlog.h>

namespace VoxelEngine {

// Static member initialization
BlockNew* BlockRegistryNew::AIR = nullptr;
BlockNew* BlockRegistryNew::STONE = nullptr;
BlockNew* BlockRegistryNew::STONE_SLAB = nullptr;

uint16_t BlockRegistryNew::m_nextStateId = 0;
std::unordered_map<std::string, BlockNew*> BlockRegistryNew::m_blocks;

void BlockRegistryNew::init() {
    spdlog::info("Initializing BlockRegistryNew...");

    // Register blocks - name comes from here!
    AIR = registerBlock<AirBlock>("air");
    STONE = registerBlock<SimpleBlock>("stone");
    STONE_SLAB = registerBlock<SlabBlock>("stone_slab");

    spdlog::info("Registered {} blocks with {} total states",
                 m_blocks.size(), m_nextStateId);
}

void BlockRegistryNew::cleanup() {
    for (auto& [name, block] : m_blocks) {
        delete block;
    }
    m_blocks.clear();
}

BlockNew* BlockRegistryNew::getBlock(BlockStateNew state) {
    for (auto& [name, block] : m_blocks) {
        if (block->hasState(state.id)) {
            return block;
        }
    }
    return nullptr;
}

BlockNew* BlockRegistryNew::getBlock(const std::string& name) {
    auto it = m_blocks.find(name);
    return (it != m_blocks.end()) ? it->second : nullptr;
}

bool BlockRegistryNew::isFaceOpaque(BlockStateNew state, Face face) {
    BlockNew* block = getBlock(state);
    return block ? block->isFaceOpaque(state, face) : false;
}

bool BlockRegistryNew::isSolid(BlockStateNew state) {
    BlockNew* block = getBlock(state);
    return block ? block->isSolid() : false;
}

bool BlockRegistryNew::isFullCube(BlockStateNew state) {
    BlockNew* block = getBlock(state);
    return block ? block->isFullCube() : false;
}

// Rendering query - integrates with existing BlockModel system
const BlockModel* BlockRegistryNew::getModel(BlockStateNew state) {
    // TODO: Implement model loading/caching
    // This will be integrated with the existing BlockModelManager
    return nullptr;
}

} // namespace VoxelEngine