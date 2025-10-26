#pragma once
#include "../BlockNew.hpp"

namespace VoxelEngine {

// SimpleBlock - regular full cube block (stone, dirt, etc.)
class SimpleBlock : public BlockNew {
public:
    SimpleBlock(const std::string& name) : BlockNew(name) {}

    // Uses all default behavior from BlockNew:
    // - All faces opaque
    // - Is solid
    // - Is full cube
    // - 1 state
};

} // namespace VoxelEngine