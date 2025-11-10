#include "BlockRegistry.hpp"
#include "BlockModel.hpp"
#include <spdlog/spdlog.h>

namespace FarHorizon {

// Static member initialization
Block* BlockRegistry::AIR = nullptr;
Block* BlockRegistry::STONE = nullptr;
Block* BlockRegistry::STONE_SLAB = nullptr;
Block* BlockRegistry::OAK_STAIRS = nullptr;
Block* BlockRegistry::GRASS_BLOCK = nullptr;

uint16_t BlockRegistry::m_nextStateId = 0;
std::unordered_map<std::string, std::unique_ptr<Block>> BlockRegistry::m_blocks;
std::unordered_map<const Block*, const BlockSoundGroup*> BlockRegistry::m_soundGroups;

void BlockRegistry::init() {
    spdlog::info("Initializing BlockRegistry...");

    // Register blocks with their sound groups - compile-time association!
    AIR = registerBlock<AirBlock>("air", BlockSoundGroup::INTENTIONALLY_EMPTY);
    STONE = registerBlock<SimpleBlock>("stone", BlockSoundGroup::STONE);
    STONE_SLAB = registerBlock<SlabBlock>("stone_slab", BlockSoundGroup::STONE);
    OAK_STAIRS = registerBlock<StairBlock>("oak_stairs", BlockSoundGroup::WOOD);
    GRASS_BLOCK = registerBlock<GrassBlock>("grass_block", BlockSoundGroup::GRASS);

    spdlog::info("Registered {} blocks with {} total states",
                 m_blocks.size(), m_nextStateId);
}

void BlockRegistry::cleanup() {
    m_blocks.clear();
    m_soundGroups.clear();
}

const BlockSoundGroup& BlockRegistry::getSoundGroup(BlockState state) {
    Block* block = getBlock(state);
    if (block) {
        auto it = m_soundGroups.find(block);
        if (it != m_soundGroups.end()) {
            return *it->second;
        }
    }
    // Fallback to stone if not found
    return BlockSoundGroup::STONE;
}

Block* BlockRegistry::getBlock(BlockState state) {
    for (auto& [name, block] : m_blocks) {
        if (block->hasState(state.id)) {
            return block.get();
        }
    }
    return nullptr;
}

Block* BlockRegistry::getBlock(const std::string& name) {
    auto it = m_blocks.find(name);
    return (it != m_blocks.end()) ? it->second.get() : nullptr;
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

const std::unordered_map<std::string, std::unique_ptr<Block>>& BlockRegistry::getAllBlocks() {
    return m_blocks;
}

} // namespace FarHorizon