#include "BlockSoundGroup.hpp"

namespace FarHorizon {

const BlockSoundGroup BlockSoundGroup::INTENTIONALLY_EMPTY = BlockSoundGroup(
    1.0f,
    1.0f,
    "intentionally_empty",
    "intentionally_empty",
    "intentionally_empty",
    "intentionally_empty",
    "intentionally_empty"
);

const BlockSoundGroup BlockSoundGroup::GRASS = BlockSoundGroup(
    1.0f,
    1.0f,
    "block.grass.break",
    "block.grass.step",
    "block.grass.place",
    "block.grass.hit",
    "block.grass.fall"
);

const BlockSoundGroup BlockSoundGroup::STONE = BlockSoundGroup(
    1.0f,
    1.0f,
    "block.stone.break",
    "block.stone.step",
    "block.stone.place",
    "block.stone.hit",
    "block.stone.fall"
);

const BlockSoundGroup BlockSoundGroup::WOOD = BlockSoundGroup(
    1.0f,
    1.0f,
    "block.wood.break",
    "block.wood.step",
    "block.wood.place",
    "block.wood.hit",
    "block.wood.fall"
);

} // namespace FarHorizon