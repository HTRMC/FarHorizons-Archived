#pragma once
#include "../BlockNew.hpp"

namespace VoxelEngine {

// AirBlock - invisible, non-solid block
class AirBlock : public BlockNew {
public:
    AirBlock(const std::string& name) : BlockNew(name) {}

    // Air is never opaque
    bool isFaceOpaque(BlockStateNew state, Face face) const override {
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

    // No properties, just 1 state
    size_t getStateCount() const override {
        return 1;
    }
};

} // namespace VoxelEngine