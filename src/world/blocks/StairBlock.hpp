#pragma once
#include "../Block.hpp"
#include "../Properties.hpp"
#include "enums/BlockHalf.hpp"
#include "enums/StairFacing.hpp"
#include "enums/StairShape.hpp"

namespace FarHorizon {

// StairBlock - stair block with facing, half, and shape properties
class StairBlock : public Block {
public:
    StairBlock(const std::string& name) : Block(name) {}

    // Stairs are never fully opaque on any face
    bool isFaceOpaque(BlockState state, Face face) const override {
        return false;
    }

    bool isFullCube() const override {
        return false;  // Stairs aren't full cubes
    }

    bool isSolid() const override {
        return true;  // Stairs are solid
    }

    // State count automatically calculated: 4 facings × 2 halves × 5 shapes = 40

    // Expose properties for model loading
    // Order: FACING, HALF, SHAPE (determines state index calculation)
    std::vector<PropertyBase*> getProperties() const override {
        return {
            const_cast<PropertyBase*>(static_cast<const PropertyBase*>(&Properties::STAIR_FACING)),
            const_cast<PropertyBase*>(static_cast<const PropertyBase*>(&Properties::BLOCK_HALF)),
            const_cast<PropertyBase*>(static_cast<const PropertyBase*>(&Properties::STAIR_SHAPE))
        };
    }

    // Helper to get specific state
    // State index = facing + half * 4 + shape * 8
    BlockState withFacingHalfAndShape(StairFacing facing, BlockHalf half, StairShape shape) const {
        uint16_t offset = static_cast<uint16_t>(facing) +
                         static_cast<uint16_t>(half) * 4 +
                         static_cast<uint16_t>(shape) * 8;
        return BlockState(m_baseStateId + offset);
    }

    // Helper for simple placement (always straight shape for now)
    BlockState withFacingAndHalf(StairFacing facing, BlockHalf half) const {
        return withFacingHalfAndShape(facing, half, StairShape::STRAIGHT);
    }

    // Override outline shape for stairs (simplified bounding box)
    BlockShape getOutlineShape(BlockState state) const override {
        // For now, use a simplified bounding box that covers the entire block space
        // The actual visual model will be handled by the blockstate JSON
        return BlockShape::partial(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f));
    }

private:
    // Decode facing, half, and shape from state ID
    StairFacing getFacing(BlockState state) const {
        int offset = state.id - m_baseStateId;
        return static_cast<StairFacing>(offset % 4);
    }

    BlockHalf getHalf(BlockState state) const {
        int offset = state.id - m_baseStateId;
        return static_cast<BlockHalf>((offset / 4) % 2);
    }

    StairShape getShape(BlockState state) const {
        int offset = state.id - m_baseStateId;
        return static_cast<StairShape>(offset / 8);
    }
};

} // namespace FarHorizon