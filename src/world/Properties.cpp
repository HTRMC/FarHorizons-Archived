#include "Properties.hpp"

namespace FarHorizon {

// Initialize static properties
Property<bool> Properties::SNOWY("snowy", {
    {"false", false},
    {"true", true}
});

Property<SlabType> Properties::SLAB_TYPE("type", {
    {"bottom", SlabType::BOTTOM},
    {"top", SlabType::TOP},
    {"double", SlabType::DOUBLE}
});

Property<BlockHalf> Properties::BLOCK_HALF("half", {
    {"bottom", BlockHalf::BOTTOM},
    {"top", BlockHalf::TOP}
});

Property<StairFacing> Properties::STAIR_FACING("facing", {
    {"north", StairFacing::NORTH},
    {"south", StairFacing::SOUTH},
    {"west", StairFacing::WEST},
    {"east", StairFacing::EAST}
});

Property<StairShape> Properties::STAIR_SHAPE("shape", {
    {"straight", StairShape::STRAIGHT},
    {"inner_left", StairShape::INNER_LEFT},
    {"inner_right", StairShape::INNER_RIGHT},
    {"outer_left", StairShape::OUTER_LEFT},
    {"outer_right", StairShape::OUTER_RIGHT}
});

} // namespace FarHorizon