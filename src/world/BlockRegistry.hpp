#pragma once
#include "Block.hpp"
#include "blocks/AirBlock.hpp"
#include "blocks/SimpleBlock.hpp"
#include "blocks/SlabBlock.hpp"
#include "BlockModel.hpp"
#include <unordered_map>
#include <string>
#include <memory>

namespace VoxelEngine {

class BlockRegistry {
public:
    // Block instances (owned by registry)
    static Block* AIR;
    static Block* STONE;
    static Block* STONE_SLAB;

    // Initialize all blocks
    static void init();

    // Cleanup
    static void cleanup();

    // Query methods
    static Block* getBlock(BlockState state);
    static Block* getBlock(const std::string& name);

    // Game logic queries (delegates to Block)
    static bool isFaceOpaque(BlockState state, Face face);
    static bool isSolid(BlockState state);
    static bool isFullCube(BlockState state);

    // Get all registered blocks (for iteration)
    static const std::unordered_map<std::string, Block*>& getAllBlocks();

private:
    static uint16_t m_nextStateId;
    static std::unordered_map<std::string, Block*> m_blocks;

    // Register a block and assign state IDs
    template<typename T>
    static Block* registerBlock(const std::string& name) {
        T* block = new T(name);
        block->m_baseStateId = m_nextStateId;

        // Reserve state IDs for this block
        m_nextStateId += static_cast<uint16_t>(block->getStateCount());

        m_blocks[name] = block;
        return block;
    }
};

} // namespace VoxelEngine