#pragma once
#include "BlockNew.hpp"
#include "blocks/AirBlock.hpp"
#include "blocks/SimpleBlock.hpp"
#include "blocks/SlabBlock.hpp"
#include "BlockModel.hpp"
#include <unordered_map>
#include <string>
#include <memory>

namespace VoxelEngine {

class BlockRegistryNew {
public:
    // Block instances (owned by registry)
    static BlockNew* AIR;
    static BlockNew* STONE;
    static BlockNew* STONE_SLAB;

    // Initialize all blocks
    static void init();

    // Cleanup
    static void cleanup();

    // Query methods
    static BlockNew* getBlock(BlockStateNew state);
    static BlockNew* getBlock(const std::string& name);

    // Game logic queries (delegates to Block)
    static bool isFaceOpaque(BlockStateNew state, Face face);
    static bool isSolid(BlockStateNew state);
    static bool isFullCube(BlockStateNew state);

    // Rendering queries
    static const BlockModel* getModel(BlockStateNew state);

private:
    static uint16_t m_nextStateId;
    static std::unordered_map<std::string, BlockNew*> m_blocks;

    // Register a block and assign state IDs
    template<typename T>
    static BlockNew* registerBlock(const std::string& name) {
        T* block = new T(name);
        block->m_baseStateId = m_nextStateId;

        // Reserve state IDs for this block
        m_nextStateId += static_cast<uint16_t>(block->getStateCount());

        m_blocks[name] = block;
        return block;
    }
};

} // namespace VoxelEngine