#pragma once
#include "../Block.hpp"
#include "../Properties.hpp"
#include "enums/SlabType.hpp"

namespace FarHorizon {

// SlabBlock - half-height block with directional opacity
class SlabBlock : public Block {
public:
    SlabBlock(const std::string& name) : Block(name) {}

    // Slabs have directional opacity based on type
    bool isFaceOpaque(BlockState state, Face face) const override {
        SlabType type = getSlabType(state);

        switch (type) {
            case SlabType::BOTTOM:
                return face == Face::DOWN;  // Only bottom face is opaque

            case SlabType::TOP:
                return face == Face::UP;    // Only top face is opaque

            case SlabType::DOUBLE:
                return true;                // All faces opaque (full block)
        }

        return false;
    }

    bool isFullCube() const override {
        return false;  // Slabs aren't full cubes (except double, but we simplify here)
    }

    bool isSolid() const override {
        return true;  // Slabs are solid
    }

    // Slabs have 3 states (bottom, top, double)
    size_t getStateCount() const override {
        return 3;
    }

    // Expose properties for model loading
    std::vector<PropertyBase*> getProperties() const override {
        return {const_cast<PropertyBase*>(static_cast<const PropertyBase*>(&Properties::SLAB_TYPE))};
    }

    // Helper to get specific state
    BlockState withType(SlabType type) const {
        return BlockState(m_baseStateId + static_cast<uint16_t>(type));
    }

    // Override outline shape for slabs
    BlockShape getOutlineShape(BlockState state) const override {
        SlabType type = getSlabType(state);

        switch (type) {
            case SlabType::BOTTOM:
                return BlockShape::partial(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.5f, 1.0f));
            case SlabType::TOP:
                return BlockShape::partial(glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f));
            case SlabType::DOUBLE:
                return BlockShape::fullCube();
        }

        return BlockShape::fullCube();
    }

private:
    // Decode slab type from state ID
    SlabType getSlabType(BlockState state) const {
        int offset = state.id - m_baseStateId;
        return static_cast<SlabType>(offset);
    }
};

} // namespace FarHorizon