#pragma once
#include "BlockState.hpp"
#include "Property.hpp"
#include "BlockRenderType.hpp"
#include "BlockShape.hpp"
#include "FaceDirection.hpp"
#include <string>
#include <cstdint>

namespace FarHorizon {

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

    // Calculate number of states dynamically from properties
    // Override only if you need custom state counting logic
    virtual size_t getStateCount() const {
        auto props = getProperties();
        if (props.empty()) {
            return 1; // Simple blocks have 1 state
        }

        // Multiply all property value counts
        size_t count = 1;
        for (PropertyBase* prop : props) {
            count *= prop->getNumValues();
        }
        return count;
    }

    // Get all properties for this block (for model loading)
    // Override in blocks with properties
    virtual std::vector<PropertyBase*> getProperties() const {
        return {}; // Simple blocks have no properties
    }

    // Get the outline shape for rendering the block selection outline
    // Override in blocks with custom shapes (slabs, stairs, etc.)
    virtual BlockShape getOutlineShape(BlockState state) const {
        return BlockShape::fullCube(); // Most blocks are full cubes
    }

    // Get the collision shape for physics collision detection
    // Based on AbstractBlock.java line 315
    // By default, uses outline shape if solid, or empty if not solid
    virtual BlockShape getCollisionShape(BlockState state) const {
        return isSolid() ? getOutlineShape(state) : BlockShape::empty();
    }

    // Check if a face should be invisible when adjacent to another block
    // Override in transparent blocks (glass, water, etc.) to implement special culling
    //
    // currentState: The block state we're rendering
    // neighborState: The adjacent block state in the given direction
    // direction: Which face we're checking
    //
    // Returns: true if the face should be culled (made invisible)
    virtual bool isSideInvisible(BlockState currentState, BlockState neighborState, FaceDirection direction) const {
        return false; // Base implementation: never cull (always draw faces)
    }

    // Sound groups are now managed by BlockRegistry (no virtual call!)
    // See BlockRegistry::getSoundGroup(BlockState) for compile-time lookup
};

} // namespace FarHorizon