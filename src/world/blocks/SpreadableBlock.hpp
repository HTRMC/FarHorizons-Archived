#pragma once
#include "SnowyBlock.hpp"

namespace VoxelEngine {

// SpreadableBlock - base class for blocks that can spread (grass, mycelium, etc.)
// Similar to Minecraft's SpreadableBlock
// This is an abstract class that provides the snowy property behavior
class SpreadableBlock : public SnowyBlock {
public:
    SpreadableBlock(const std::string& name) : SnowyBlock(name) {}

    // TODO: Add spreading logic in future (randomTick, canSurvive, canSpread)
    // For now, this is just a base class that inherits snowy behavior
};

} // namespace VoxelEngine