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
Block* BlockRegistry::GLASS = nullptr;

uint16_t BlockRegistry::nextStateId_ = 0;
std::unordered_map<std::string, std::unique_ptr<Block>> BlockRegistry::blocks_;
std::unordered_map<const Block*, const BlockSoundGroup*> BlockRegistry::soundGroups_;

void BlockRegistry::init() {
    spdlog::info("Initializing BlockRegistry...");

    // Register blocks with their sound groups - compile-time association!
    AIR = registerBlock<AirBlock>("air", BlockSoundGroup::INTENTIONALLY_EMPTY);
    STONE = registerBlock<SimpleBlock>("stone", BlockSoundGroup::STONE);
    STONE_SLAB = registerBlock<SlabBlock>("stone_slab", BlockSoundGroup::STONE);
    OAK_STAIRS = registerBlock<StairBlock>("oak_stairs", BlockSoundGroup::WOOD);
    GRASS_BLOCK = registerBlock<GrassBlock>("grass_block", BlockSoundGroup::GRASS);
    GLASS = registerBlock<TransparentBlock>("glass", BlockSoundGroup::GLASS);

    spdlog::info("Registered {} blocks with {} total states",
                 blocks_.size(), nextStateId_);
}

void BlockRegistry::cleanup() {
    blocks_.clear();
    soundGroups_.clear();
}

const BlockSoundGroup& BlockRegistry::getSoundGroup(BlockState state) {
    Block* block = getBlock(state);
    if (block) {
        auto it = soundGroups_.find(block);
        if (it != soundGroups_.end()) {
            return *it->second;
        }
    }
    // Fallback to stone if not found
    return BlockSoundGroup::STONE;
}

Block* BlockRegistry::getBlock(BlockState state) {
    for (auto& [name, block] : blocks_) {
        if (block->hasState(state.id)) {
            return block.get();
        }
    }
    return nullptr;
}

Block* BlockRegistry::getBlock(const std::string& name) {
    auto it = blocks_.find(name);
    return (it != blocks_.end()) ? it->second.get() : nullptr;
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
    return blocks_;
}

} // namespace FarHorizon