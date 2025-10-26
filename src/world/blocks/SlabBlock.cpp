#include "SlabBlock.hpp"

namespace VoxelEngine {

// Initialize static property
Property<SlabType> SlabBlock::TYPE("type", {
    {"bottom", SlabType::BOTTOM},
    {"top", SlabType::TOP},
    {"double", SlabType::DOUBLE}
});

} // namespace VoxelEngine