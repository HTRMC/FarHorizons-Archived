#pragma once
#include <string>

namespace FarHorizon {

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
    static const BlockSoundGroup STONE;
    static const BlockSoundGroup GRASS;

};

} // namespace FarHorizon
