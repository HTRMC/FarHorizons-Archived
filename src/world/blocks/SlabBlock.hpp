#pragma once
#include "../BlockNew.hpp"

namespace VoxelEngine {

// Slab type property values
enum class SlabType : uint8_t {
    BOTTOM = 0,
    TOP = 1,
    DOUBLE = 2
};

// SlabBlock - half-height block with directional opacity
class SlabBlock : public BlockNew {
public:
    // Static property definition (shared by all slab instances)
    static Property<SlabType> TYPE;

    SlabBlock(const std::string& name) : BlockNew(name) {}

    // Slabs have directional opacity based on type
    bool isFaceOpaque(BlockStateNew state, Face face) const override {
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
        return {const_cast<PropertyBase*>(static_cast<const PropertyBase*>(&TYPE))};
    }

    // Helper to get specific state
    BlockStateNew withType(SlabType type) const {
        return BlockStateNew(m_baseStateId + static_cast<uint16_t>(type));
    }

private:
    // Decode slab type from state ID
    SlabType getSlabType(BlockStateNew state) const {
        int offset = state.id - m_baseStateId;
        return static_cast<SlabType>(offset);
    }
};

// Static property initialization (will be defined in .cpp)
// Property<SlabType> SlabBlock::TYPE("type", {
//     {"bottom", SlabType::BOTTOM},
//     {"top", SlabType::TOP},
//     {"double", SlabType::DOUBLE}
// });

} // namespace VoxelEngine