#include "BlockState.hpp"
#include <sstream>

namespace VoxelEngine {

// BlockState Implementation

BlockState::BlockState(BlockType type, const std::string& variant)
    : m_type(type), m_variant(variant) {}

std::string BlockState::getModelName() const {
    std::string baseName = std::string(getBlockName(m_type));
    if (m_variant.empty()) {
        return baseName;
    }
    return baseName + "_" + m_variant;
}

bool BlockState::operator==(const BlockState& other) const {
    return m_type == other.m_type && m_variant == other.m_variant;
}

std::size_t BlockState::Hash::operator()(const BlockState& state) const {
    // Combine hash of type and variant
    std::size_t h1 = std::hash<uint8_t>()(static_cast<uint8_t>(state.m_type));
    std::size_t h2 = std::hash<std::string>()(state.m_variant);
    return h1 ^ (h2 << 1);
}

// BlockStateRegistry Implementation

BlockStateRegistry::BlockStateRegistry() {
    // Reserve ID 0 for AIR
    m_states.reserve(256);  // Pre-allocate for common case

    // Register AIR as the first state (ID = 0)
    BlockState airState(BlockType::AIR);
    m_states.push_back(airState);
    m_stateToId[airState] = AIR_ID;
}

BlockStateRegistry& BlockStateRegistry::getInstance() {
    static BlockStateRegistry instance;
    return instance;
}

BlockStateRegistry::BlockStateId BlockStateRegistry::registerBlockState(const BlockState& state) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if already registered
    auto it = m_stateToId.find(state);
    if (it != m_stateToId.end()) {
        return it->second;
    }

    // Create new ID
    BlockStateId newId = static_cast<BlockStateId>(m_states.size());
    m_states.push_back(state);
    m_stateToId[state] = newId;

    return newId;
}

const BlockState& BlockStateRegistry::getBlockState(BlockStateId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (id >= m_states.size()) {
        // Return AIR state for invalid IDs
        return m_states[AIR_ID];
    }

    return m_states[id];
}

BlockStateRegistry::BlockStateId BlockStateRegistry::getOrCreateId(BlockType type, const std::string& variant) {
    BlockState state(type, variant);
    return registerBlockState(state);
}

BlockStateRegistry::BlockStateId BlockStateRegistry::getId(const BlockState& state) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_stateToId.find(state);
    if (it != m_stateToId.end()) {
        return it->second;
    }

    return INVALID_ID;
}

void BlockStateRegistry::initializeDefaultStates() {
    // Register default state for each block type
    registerBlockState(BlockState(BlockType::AIR, ""));
    registerBlockState(BlockState(BlockType::STONE, ""));

    // Register stone slab variants
    registerBlockState(BlockState(BlockType::STONE_SLAB, ""));      // Default/bottom
    registerBlockState(BlockState(BlockType::STONE_SLAB, "top"));   // Top slab

    // Add more block variants here as needed
}

size_t BlockStateRegistry::getStateCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_states.size();
}

} // namespace VoxelEngine