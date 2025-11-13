#pragma once
#include "Block.hpp"
#include "blocks/AirBlock.hpp"
#include "blocks/SimpleBlock.hpp"
#include "blocks/SlabBlock.hpp"
#include "blocks/StairBlock.hpp"
#include "blocks/GrassBlock.hpp"
#include "blocks/TransparentBlock.hpp"
#include "BlockModel.hpp"
#include "BlockSoundGroup.hpp"
#include <unordered_map>
#include <string>
#include <memory>

namespace FarHorizon {

class BlockRegistry {
public:
    // Block instances (owned by registry)
    static Block* AIR;
    static Block* STONE;
    static Block* STONE_SLAB;
    static Block* OAK_STAIRS;
    static Block* GRASS_BLOCK;
    static Block* GLASS;

    // Initialize all blocks
    static void init();

    // Cleanup
    static void cleanup();

    // Query methods
    static Block* getBlock(BlockState state);
    static Block* getBlock(const std::string& name);

    // Sound system - get sound group for a block state (no virtual call needed!)
    static const BlockSoundGroup& getSoundGroup(BlockState state);

    // Game logic queries (delegates to Block)
    static bool isFaceOpaque(BlockState state, Face face);
    static bool isSolid(BlockState state);
    static bool isFullCube(BlockState state);

    // Get all registered blocks (for iteration)
    static const std::unordered_map<std::string, std::unique_ptr<Block>>& getAllBlocks();

private:
    static uint16_t nextStateId_;
    static std::unordered_map<std::string, std::unique_ptr<Block>> blocks_;

    // Map from block pointer to sound group (compile-time lookup, no virtual calls!)
    static std::unordered_map<const Block*, const BlockSoundGroup*> soundGroups_;

    // Register a block and assign state IDs
    template<typename T>
    static Block* registerBlock(const std::string& name, const BlockSoundGroup& soundGroup) {
        auto block = std::make_unique<T>(name);
        block->baseStateId_ = nextStateId_;

        // Reserve state IDs for this block
        nextStateId_ += static_cast<uint16_t>(block->getStateCount());

        Block* blockPtr = block.get();
        blocks_[name] = std::move(block);

        // Register sound group (no virtual call needed at runtime!)
        soundGroups_[blockPtr] = &soundGroup;

        return blockPtr;
    }
};

} // namespace FarHorizon