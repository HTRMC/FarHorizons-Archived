#pragma once
#include "../Block.hpp"

namespace FarHorizon {

// SimpleBlock - regular full cube block (stone, dirt, etc.)
class SimpleBlock : public Block {
public:
    SimpleBlock(const std::string& name) : Block(name) {}

    // Uses all default behavior from BlockNew:
    // - All faces opaque
    // - Is solid
    // - Is full cube
    // - 1 state
};

} // namespace FarHorizon