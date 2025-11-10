#pragma once
#include "../Block.hpp"
#include "../Properties.hpp"

namespace FarHorizon {

// SnowyBlock - base class for blocks that can have snow on top
// Similar to Minecraft's SnowyBlock
class SnowyBlock : public Block {
public:
    SnowyBlock(const std::string& name) : Block(name) {}

    // Full cube with all faces opaque
    bool isFaceOpaque(BlockState state, Face face) const override {
        return true;
    }

    bool isFullCube() const override {
        return true;
    }

    bool isSolid() const override {
        return true;
    }

    // Has 2 states (snowy=false, snowy=true)
    size_t getStateCount() const override {
        return 2;
    }

    // Expose SNOWY property
    std::vector<PropertyBase*> getProperties() const override {
        return {const_cast<PropertyBase*>(static_cast<const PropertyBase*>(&Properties::SNOWY))};
    }

    // Helper to get state with specific snowy value
    BlockState withSnowy(bool snowy) const {
        return BlockState(m_baseStateId + (snowy ? 1 : 0));
    }

protected:
    // Decode snowy state from BlockState
    bool isSnowy(BlockState state) const {
        int offset = state.id - m_baseStateId;
        return offset == 1;
    }
};

} // namespace FarHorizon