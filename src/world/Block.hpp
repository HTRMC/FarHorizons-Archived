#pragma once
#include "BlockState.hpp"
#include "Property.hpp"
#include "BlockRenderType.hpp"
#include <string>
#include <cstdint>

namespace VoxelEngine {

// Block base class - defines game logic behavior
class Block {
public:
    std::string m_name;
    uint16_t m_baseStateId;

    Block(const std::string& name) : m_name(name), m_baseStateId(0) {}
    virtual ~Block() = default;

    // Game logic queries - override in subclasses
    virtual bool isFaceOpaque(BlockState state, Face face) const {
        return true; // Full blocks are opaque on all faces
    }

    virtual bool isSolid() const {
        return true; // Most blocks are solid
    }

    virtual bool isFullCube() const {
        return true; // Most blocks are full cubes
    }

    virtual bool hasBlockEntity() const {
        return false; // Most blocks don't have block entities
    }

    // Rendering queries - override in blocks like AirBlock
    virtual BlockRenderType getRenderType(BlockState state) const {
        return BlockRenderType::MODEL; // Most blocks have models
    }

    // Get the default state (base state with all properties at default)
    BlockState getDefaultState() const {
        return BlockState(m_baseStateId);
    }

    // Check if a state ID belongs to this block
    bool hasState(uint16_t stateId) const {
        return stateId >= m_baseStateId && stateId < m_baseStateId + getStateCount();
    }

    // Override to define properties (called during registration)
    virtual void defineProperties() {}

    // Override to return number of states this block has
    virtual size_t getStateCount() const {
        return 1; // Simple blocks have 1 state
    }

    // Get all properties for this block (for model loading)
    // Override in blocks with properties
    virtual std::vector<PropertyBase*> getProperties() const {
        return {}; // Simple blocks have no properties
    }
};

} // namespace VoxelEngine