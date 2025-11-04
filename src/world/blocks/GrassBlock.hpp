#pragma once
#include "SpreadableBlock.hpp"

namespace VoxelEngine {

// GrassBlock - grass block with snowy property
// Extends SpreadableBlock which extends SnowyBlock which has the SNOWY property
class GrassBlock : public SpreadableBlock {
public:
    GrassBlock(const std::string& name) : SpreadableBlock(name) {}

    // Inherits all behavior from SpreadableBlock:
    // - 2 states (snowy=false, snowy=true)
    // - Full cube, solid, opaque faces
    // - SNOWY property from Properties class

    // Override sound type for grass blocks
    std::string getSoundType() const override {
        return "grass";
    }

    // TODO: Add fertilizable behavior in future (grow grass/flowers on top)
};

} // namespace VoxelEngine