#pragma once
#include "world/Block.hpp"

namespace VoxelEngine {

// AirBlock - invisible, non-solid block
class AirBlock : public Block {
public:
    AirBlock(const std::string& name) : Block(name) {}

    // Air is never opaque
    bool isFaceOpaque(BlockState state, Face face) const override {
        return false;
    }

    // Air is not solid
    bool isSolid() const override {
        return false;
    }

    // Air is not a full cube
    bool isFullCube() const override {
        return false;
    }

    // Air has no model - skip rendering entirely
    BlockRenderType getRenderType(BlockState state) const override {
        return BlockRenderType::INVISIBLE;
    }

    // No properties, just 1 state
    size_t getStateCount() const override {
        return 1;
    }
};

} // namespace VoxelEngine