#pragma once

#include "BlockType.hpp"
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>

namespace VoxelEngine {

// Represents a specific state of a block (e.g., stone_slab with variant="top")
class BlockState {
public:
    BlockState(BlockType type, const std::string& variant = "");

    BlockType getType() const { return m_type; }
    const std::string& getVariant() const { return m_variant; }

    // Get the model name for this blockstate (e.g., "stone_slab_top")
    std::string getModelName() const;

    bool operator==(const BlockState& other) const;

    // Hash support for use in unordered_map
    struct Hash {
        std::size_t operator()(const BlockState& state) const;
    };

private:
    BlockType m_type;
    std::string m_variant;  // e.g., "", "top", "bottom", "double"
};

// Global registry that manages all blockstates and assigns them unique IDs
class BlockStateRegistry {
public:
    using BlockStateId = uint16_t;  // Allows up to 65536 unique blockstates
    static constexpr BlockStateId INVALID_ID = 0;
    static constexpr BlockStateId AIR_ID = 0;

    static BlockStateRegistry& getInstance();

    // Register a blockstate and get its unique ID
    BlockStateId registerBlockState(const BlockState& state);

    // Get the blockstate for a given ID
    const BlockState& getBlockState(BlockStateId id) const;

    // Get or create a blockstate ID
    BlockStateId getOrCreateId(BlockType type, const std::string& variant = "");

    // Get ID for an existing blockstate (returns INVALID_ID if not registered)
    BlockStateId getId(const BlockState& state) const;

    // Initialize default blockstates for all block types
    void initializeDefaultStates();

    // Get total number of registered states
    size_t getStateCount() const;

private:
    BlockStateRegistry();
    BlockStateRegistry(const BlockStateRegistry&) = delete;
    BlockStateRegistry& operator=(const BlockStateRegistry&) = delete;

    mutable std::mutex m_mutex;
    std::vector<BlockState> m_states;  // Index = BlockStateId
    std::unordered_map<BlockState, BlockStateId, BlockState::Hash> m_stateToId;
};

} // namespace VoxelEngine