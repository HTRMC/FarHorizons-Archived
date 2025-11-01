#include "Properties.hpp"

namespace VoxelEngine {

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

} // namespace VoxelEngine