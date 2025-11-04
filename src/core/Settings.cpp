#include "Settings.hpp"

namespace VoxelEngine {

Settings::Settings()
    : version(ofInt("version", 1, 1, 100))
    , fov(ofFloat("fov", 70.0f, 30.0f, 110.0f))
    , renderDistance(ofInt("renderDistance", 8, 2, 32))
    , enableVsync(ofBoolean("enableVsync", true))
    , fullscreen(ofBoolean("fullscreen", false))
    , guiScale(ofInt("guiScale", 0, 0, 6))
    , maxFps(ofInt("maxFps", 260, 10, 260))
    , mipmapLevels(ofInt("mipmapLevels", 2, 0, 4))
    , menuBlurAmount(ofInt("menuBlurAmount", 1, 0, 10))
    , renderClouds(ofBoolean("renderClouds", false))
    , cloudRange(ofInt("cloudRange", 128, 2, 128))
    , soundDevice(ofString("soundDevice", ""))
    , masterVolume(ofFloat("masterVolume", 0.5f, 0.0f, 1.0f))
    , saveChatDrafts(ofBoolean("saveChatDrafts", false))
    , mouseSensitivity(ofFloat("mouseSensitivity", 0.5f, 0.0f, 1.0f))
{
}

} // namespace VoxelEngine