#pragma once
#include <string>

namespace VoxelEngine {

class BlockSoundGroup {
public:
    const float volume;
    const float pitch;
    const std::string breakSound;
    const std::string stepSound;
    const std::string placeSound;
    const std::string hitSound;
    const std::string fallSound;

    constexpr BlockSoundGroup(
        float volume,
        float pitch,
        const std::string& breakSound,
        const std::string& stepSound,
        const std::string& placeSound,
        const std::string& hitSound,
        const std::string& fallSound
    ) : volume(volume),
        pitch(pitch),
        breakSound(breakSound),
        stepSound(stepSound),
        placeSound(placeSound),
        hitSound(hitSound),
        fallSound(fallSound) {}

    // Getters
    constexpr float getVolume() const { return volume; }
    constexpr float getPitch() const { return pitch; }
    constexpr const std::string& getBreakSound() const { return breakSound; }
    constexpr const std::string& getStepSound() const { return stepSound; }
    constexpr const std::string& getPlaceSound() const { return placeSound; }
    constexpr const std::string& getHitSound() const { return hitSound; }
    constexpr const std::string& getFallSound() const { return fallSound; }

    // ===== Static Sound Group Constants =====
    // These mirror Minecraft's BlockSoundGroup definitions

    static const BlockSoundGroup INTENTIONALLY_EMPTY;
    static const BlockSoundGroup WOOD;
    static const BlockSoundGroup GRAVEL;
    static const BlockSoundGroup GRASS;
    static const BlockSoundGroup STONE;
    static const BlockSoundGroup METAL;
    static const BlockSoundGroup GLASS;
    static const BlockSoundGroup WOOL;
    static const BlockSoundGroup SAND;
    static const BlockSoundGroup SNOW;
    static const BlockSoundGroup LADDER;
    static const BlockSoundGroup ANVIL;
    static const BlockSoundGroup SLIME;
    static const BlockSoundGroup WET_GRASS;
    static const BlockSoundGroup CORAL;
    static const BlockSoundGroup BAMBOO;
    static const BlockSoundGroup BAMBOO_SAPLING;
    static const BlockSoundGroup SCAFFOLDING;
    static const BlockSoundGroup SWEET_BERRY_BUSH;
    static const BlockSoundGroup CROP;
    static const BlockSoundGroup STEM;
    static const BlockSoundGroup VINE;
    static const BlockSoundGroup NETHER_WART;
    static const BlockSoundGroup LANTERN;
    static const BlockSoundGroup NETHER_STEM;
    static const BlockSoundGroup NYLIUM;
    static const BlockSoundGroup FUNGUS;
    static const BlockSoundGroup ROOTS;
    static const BlockSoundGroup SHROOMLIGHT;
    static const BlockSoundGroup WEEPING_VINES;
    static const BlockSoundGroup SOUL_SAND;
    static const BlockSoundGroup SOUL_SOIL;
    static const BlockSoundGroup BASALT;
    static const BlockSoundGroup WART_BLOCK;
    static const BlockSoundGroup NETHERRACK;
    static const BlockSoundGroup NETHER_BRICKS;
    static const BlockSoundGroup NETHER_SPROUTS;
    static const BlockSoundGroup NETHER_ORE;
    static const BlockSoundGroup BONE;
    static const BlockSoundGroup NETHERITE;
    static const BlockSoundGroup ANCIENT_DEBRIS;
    static const BlockSoundGroup LODESTONE;
    static const BlockSoundGroup CHAIN;
    static const BlockSoundGroup NETHER_GOLD_ORE;
    static const BlockSoundGroup GILDED_BLACKSTONE;
    static const BlockSoundGroup CANDLE;
    static const BlockSoundGroup AMETHYST_BLOCK;
    static const BlockSoundGroup AMETHYST_CLUSTER;
    static const BlockSoundGroup SMALL_AMETHYST_BUD;
    static const BlockSoundGroup MEDIUM_AMETHYST_BUD;
    static const BlockSoundGroup LARGE_AMETHYST_BUD;
    static const BlockSoundGroup TUFF;
    static const BlockSoundGroup CALCITE;
    static const BlockSoundGroup DRIPSTONE_BLOCK;
    static const BlockSoundGroup POINTED_DRIPSTONE;
    static const BlockSoundGroup COPPER;
    static const BlockSoundGroup CAVE_VINES;
    static const BlockSoundGroup SPORE_BLOSSOM;
    static const BlockSoundGroup AZALEA;
    static const BlockSoundGroup FLOWERING_AZALEA;
    static const BlockSoundGroup MOSS_CARPET;
    static const BlockSoundGroup MOSS_BLOCK;
    static const BlockSoundGroup BIG_DRIPLEAF;
    static const BlockSoundGroup SMALL_DRIPLEAF;
    static const BlockSoundGroup ROOTED_DIRT;
    static const BlockSoundGroup HANGING_ROOTS;
    static const BlockSoundGroup AZALEA_LEAVES;
    static const BlockSoundGroup SCULK_SENSOR;
    static const BlockSoundGroup SCULK_CATALYST;
    static const BlockSoundGroup SCULK;
    static const BlockSoundGroup SCULK_VEIN;
    static const BlockSoundGroup SCULK_SHRIEKER;
    static const BlockSoundGroup GLOW_LICHEN;
    static const BlockSoundGroup DEEPSLATE;
    static const BlockSoundGroup DEEPSLATE_BRICKS;
    static const BlockSoundGroup DEEPSLATE_TILES;
    static const BlockSoundGroup POLISHED_DEEPSLATE;
    static const BlockSoundGroup FROGLIGHT;
    static const BlockSoundGroup FROGSPAWN;
    static const BlockSoundGroup MANGROVE_ROOTS;
    static const BlockSoundGroup MUDDY_MANGROVE_ROOTS;
    static const BlockSoundGroup MUD;
    static const BlockSoundGroup MUD_BRICKS;
    static const BlockSoundGroup PACKED_MUD;
    static const BlockSoundGroup HANGING_SIGN;
    static const BlockSoundGroup NETHER_WOOD_HANGING_SIGN;
    static const BlockSoundGroup BAMBOO_WOOD_HANGING_SIGN;
    static const BlockSoundGroup BAMBOO_WOOD;
    static const BlockSoundGroup NETHER_WOOD;
    static const BlockSoundGroup CHERRY_WOOD;
    static const BlockSoundGroup CHERRY_SAPLING;
    static const BlockSoundGroup CHERRY_LEAVES;
    static const BlockSoundGroup CHERRY_WOOD_HANGING_SIGN;
    static const BlockSoundGroup CHISELED_BOOKSHELF;
    static const BlockSoundGroup SUSPICIOUS_SAND;
    static const BlockSoundGroup SUSPICIOUS_GRAVEL;
    static const BlockSoundGroup DECORATED_POT;
    static const BlockSoundGroup TRIAL_SPAWNER;
    static const BlockSoundGroup SPONGE;
    static const BlockSoundGroup WET_SPONGE;
    static const BlockSoundGroup COBWEB;
    static const BlockSoundGroup SPAWNER;
};

} // namespace VoxelEngine
