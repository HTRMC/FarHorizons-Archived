#include "BlockRegistry.hpp"
#include "BlockModel.hpp"
#include <spdlog/spdlog.h>

namespace VoxelEngine {

// Static member initialization
Block* BlockRegistry::AIR = nullptr;
Block* BlockRegistry::STONE = nullptr;
Block* BlockRegistry::STONE_SLAB = nullptr;
Block* BlockRegistry::GRASS_BLOCK = nullptr;

uint16_t BlockRegistry::m_nextStateId = 0;
std::unordered_map<std::string, Block*> BlockRegistry::m_blocks;

void BlockRegistry::init() {
    spdlog::info("Initializing BlockRegistry...");

    // Register blocks - name comes from here!
    AIR = registerBlock<AirBlock>("air");
    STONE = registerBlock<SimpleBlock>("stone");
    STONE_SLAB = registerBlock<SlabBlock>("stone_slab");
    GRASS_BLOCK = registerBlock<GrassBlock>("grass_block");

    spdlog::info("Registered {} blocks with {} total states",
                 m_blocks.size(), m_nextStateId);
}

void BlockRegistry::cleanup() {
    for (auto& [name, block] : m_blocks) {
        delete block;
    }
    m_blocks.clear();
}

Block* BlockRegistry::getBlock(BlockState state) {
    for (auto& [name, block] : m_blocks) {
        if (block->hasState(state.id)) {
            return block;
        }
    }
    return nullptr;
}

Block* BlockRegistry::getBlock(const std::string& name) {
    auto it = m_blocks.find(name);
    return (it != m_blocks.end()) ? it->second : nullptr;
}

bool BlockRegistry::isFaceOpaque(BlockState state, Face face) {
    Block* block = getBlock(state);
    return block ? block->isFaceOpaque(state, face) : false;
}

bool BlockRegistry::isSolid(BlockState state) {
    Block* block = getBlock(state);
    return block ? block->isSolid() : false;
}

bool BlockRegistry::isFullCube(BlockState state) {
    Block* block = getBlock(state);
    return block ? block->isFullCube() : false;
}

const std::unordered_map<std::string, Block*>& BlockRegistry::getAllBlocks() {
    return m_blocks;
}

} // namespace VoxelEngine