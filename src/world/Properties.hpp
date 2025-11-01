#pragma once
#include "Property.hpp"
#include "blocks/enums/SlabType.hpp"

namespace VoxelEngine {

// Central registry of all block properties (like Minecraft's Properties class)
class Properties {
public:
    // Boolean properties
    static Property<bool> SNOWY;

    // Enum properties
    static Property<SlabType> SLAB_TYPE;

    // Add more properties as needed...
};

} // namespace VoxelEngine