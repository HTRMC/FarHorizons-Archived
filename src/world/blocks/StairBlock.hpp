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
        return BlockState(baseStateId_ + offset);
    }

    // Helper for simple placement (always straight shape for now)
    BlockState withFacingAndHalf(StairFacing facing, BlockHalf half) const {
        return withFacingHalfAndShape(facing, half, StairShape::STRAIGHT);
    }

    // Override outline shape for stairs
    // Based on Minecraft's StairBlock.java shape definitions
    BlockShape getOutlineShape(BlockState state) const override {
        BlockHalf half = getHalf(state);
        StairFacing facing = getFacing(state);
        StairShape shape = getShape(state);

        // Minecraft stair shapes (in normalized 0-1 space):
        // - OUTER: Bottom half + one top corner (L-shaped)
        // - STRAIGHT: Bottom half + top back half (normal stair)
        // - INNER: Bottom half + three top corners (U-shaped, almost full)

        // For now, use simplified bounding boxes based on shape type
        // These approximate the actual collision volume
        switch (shape) {
            case StairShape::OUTER_LEFT:
            case StairShape::OUTER_RIGHT:
                // Outer corner: smaller volume (~75% of block)
                // Approximation: bottom half + partial top
                if (half == BlockHalf::BOTTOM) {
                    return BlockShape::fromBounds(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.75f, 1.0f));
                } else {
                    return BlockShape::fromBounds(glm::vec3(0.0f, 0.25f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f));
                }

            case StairShape::INNER_LEFT:
            case StairShape::INNER_RIGHT:
                // Inner corner: almost full block (~90% volume)
                // These should NOT cull most adjacent faces
                if (half == BlockHalf::BOTTOM) {
                    return BlockShape::fromBounds(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.9f, 1.0f));
                } else {
                    return BlockShape::fromBounds(glm::vec3(0.0f, 0.1f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f));
                }

            case StairShape::STRAIGHT:
            default:
                // Straight stair: bottom half + top half (back side)
                // Approximation: 3/4 of full height
                if (half == BlockHalf::BOTTOM) {
                    return BlockShape::fromBounds(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.8f, 1.0f));
                } else {
                    return BlockShape::fromBounds(glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f));
                }
        }
    }

private:
    // Decode facing, half, and shape from state ID
    StairFacing getFacing(BlockState state) const {
        int offset = state.id - baseStateId_;
        return static_cast<StairFacing>(offset % 4);
    }

    BlockHalf getHalf(BlockState state) const {
        int offset = state.id - baseStateId_;
        return static_cast<BlockHalf>((offset / 4) % 2);
    }

    StairShape getShape(BlockState state) const {
        int offset = state.id - baseStateId_;
        return static_cast<StairShape>(offset / 8);
    }
};

} // namespace FarHorizon