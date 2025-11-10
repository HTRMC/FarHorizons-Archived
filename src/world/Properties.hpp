#pragma once
#include "Property.hpp"
#include "blocks/enums/SlabType.hpp"
#include "blocks/enums/BlockHalf.hpp"
#include "blocks/enums/StairFacing.hpp"
#include "blocks/enums/StairShape.hpp"

namespace FarHorizon {

// Central registry of all block properties (like Minecraft's Properties class)
class Properties {
public:
    // Boolean properties
    static Property<bool> SNOWY;

    // Enum properties
    static Property<SlabType> SLAB_TYPE;
    static Property<BlockHalf> BLOCK_HALF;
    static Property<StairFacing> STAIR_FACING;
    static Property<StairShape> STAIR_SHAPE;

    // Add more properties as needed...
};

} // namespace FarHorizon