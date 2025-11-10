#pragma once
#include "../Block.hpp"

namespace FarHorizon {

// GlassBlock - transparent full cube block
class TransparentBlock : public Block {
public:
    TransparentBlock(const std::string& name) : Block(name) {}

    // Override face opacity to make glass transparent
    bool isFaceOpaque(BlockState state, Face face) const override {
        return false;  // Glass faces are not opaque, allowing you to see through
    }

    // Glass is still solid (you can't walk through it)
    // Uses default: bool isSolid() const override { return true; }

    // Glass is a full cube (occupies entire block space)
    // Uses default: bool isFullCube() const override { return true; }

    // Has 1 state (no variants)
    // Uses default: uint16_t getStateCount() const override { return 1; }
};

} // namespace FarHorizon