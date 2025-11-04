#pragma once

#include <string>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <spdlog/spdlog.h>
#include <simdjson.h>
#include "SimpleOption.hpp"

namespace VoxelEngine {

/**
 * Manages persistent game settings using SimpleOption for flexibility
 * Saves to and loads from settings.json with versioning support
 */
class Settings {
public:
    Settings();

    // Settings version for compatibility
    SimpleOption<int32_t> version;

    // Video settings
    SimpleOption<float> fov;
    SimpleOption<int32_t> renderDistance;
    SimpleOption<bool> enableVsync;
    SimpleOption<bool> fullscreen;
    SimpleOption<int32_t> guiScale;  // 0 for auto, 1-6 for manual
    SimpleOption<int32_t> maxFps;
    SimpleOption<int32_t> mipmapLevels;
    SimpleOption<int32_t> menuBlurAmount;

    // Rendering options
    SimpleOption<bool> renderClouds;
    SimpleOption<int32_t> cloudRange;

    // Audio
    SimpleOption<std::string> soundDevice;

    // Resources
    std::vector<std::string> resourcePacks = {"vanilla"};

    // Chat
    SimpleOption<bool> saveChatDrafts;

    // Mouse settings (0.0 - 1.0, default 0.5 = 50% like Minecraft)
    SimpleOption<float> mouseSensitivity;

    // Keybinds (stored as string pairs: action -> key)
    std::unordered_map<std::string, std::string> keybinds = {
        {"key.attack", "key.mouse.left"},
        {"key.use", "key.mouse.right"},
        {"key.forward", "key.keyboard.w"},
        {"key.left", "key.keyboard.a"},
        {"key.back", "key.keyboard.s"},
        {"key.right", "key.keyboard.d"},
        {"key.jump", "key.keyboard.space"},
        {"key.sneak", "key.keyboard.left.shift"},
        {"key.sprint", "key.keyboard.left.control"},
        {"key.chat", "key.keyboard.t"},
        {"key.command", "key.keyboard.slash"},
        {"key.screenshot", "key.keyboard.f2"},
        {"key.togglePerspective", "key.keyboard.f5"},
        {"key.fullscreen", "key.keyboard.f11"}
    };

    /**
     * Calculate automatic GUI scale based on screen height.
     * More conservative scaling for better readability:
     * - Scale 1: < 720p (smaller screens)
     * - Scale 2: 720p - 1080p (most common)
     * - Scale 3: 1080p - 1440p
     * - Scale 4: 1440p - 2160p (4K)
     * - Scale 5: 2160p+ (4K and above)
     */
    static int32_t calculateAutoGuiScale(uint32_t screenHeight) {
        if (screenHeight < 720) {
            return 1;
        } else if (screenHeight < 1080) {
            return 2;
        } else if (screenHeight < 1440) {
            return 3;
        } else if (screenHeight < 2160) {
            return 4;
        } else {
            return 5;
        }
    }

    /**
     * Get the effective GUI scale.
     * If guiScale is 0 (Auto), returns the calculated scale for the given screen height.
     * Otherwise returns the manual guiScale setting (1-6).
     */
    int32_t getEffectiveGuiScale(uint32_t screenHeight) const {
        if (guiScale.getValue() == 0) {  // 0 = Auto
            return calculateAutoGuiScale(screenHeight);
        }
        return std::max(1, std::min(6, guiScale.getValue()));  // Clamp to valid range
    }

    /**
     * Load settings from file. Returns true if successful.
     * If file doesn't exist or is invalid, uses default values.
     */
    bool load(const std::string& filepath = "settings.json") {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            spdlog::info("Settings file not found, using defaults");
            return false;
        }

        try {
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            file.close();

            simdjson::ondemand::parser parser;
            simdjson::padded_string json(content);
            simdjson::ondemand::document doc = parser.iterate(json);
            simdjson::ondemand::object root = doc.get_object();

            // Parse each field (errors are silently ignored, keeping defaults)
            if (auto v = root.find_field("version"); !v.error()) version.deserialize(v.value());
            if (auto v = root.find_field("fov"); !v.error()) fov.deserialize(v.value());
            if (auto v = root.find_field("renderDistance"); !v.error()) renderDistance.deserialize(v.value());
            if (auto v = root.find_field("enableVsync"); !v.error()) enableVsync.deserialize(v.value());
            if (auto v = root.find_field("fullscreen"); !v.error()) fullscreen.deserialize(v.value());
            if (auto v = root.find_field("guiScale"); !v.error()) guiScale.deserialize(v.value());
            if (auto v = root.find_field("maxFps"); !v.error()) maxFps.deserialize(v.value());
            if (auto v = root.find_field("mipmapLevels"); !v.error()) mipmapLevels.deserialize(v.value());
            if (auto v = root.find_field("menuBlurAmount"); !v.error()) menuBlurAmount.deserialize(v.value());
            if (auto v = root.find_field("renderClouds"); !v.error()) renderClouds.deserialize(v.value());
            if (auto v = root.find_field("cloudRange"); !v.error()) cloudRange.deserialize(v.value());
            if (auto v = root.find_field("soundDevice"); !v.error()) soundDevice.deserialize(v.value());
            if (auto v = root.find_field("saveChatDrafts"); !v.error()) saveChatDrafts.deserialize(v.value());
            if (auto v = root.find_field("mouseSensitivity"); !v.error()) mouseSensitivity.deserialize(v.value());

            // Parse resource packs array
            if (auto arr = root.find_field("resourcePacks"); !arr.error()) {
                resourcePacks.clear();
                for (auto item : arr.get_array()) {
                    resourcePacks.push_back(std::string(item.get_string().value()));
                }
            }

            // Parse keybinds object
            if (auto kb = root.find_field("keybinds"); !kb.error()) {
                for (auto field : kb.get_object()) {
                    std::string key = std::string(field.unescaped_key().value());
                    std::string value = std::string(field.value().get_string().value());
                    keybinds[key] = value;
                }
            }

            spdlog::info("Loaded settings (v{}): FOV={}, RenderDistance={}, VSync={}",
                        version.getValue(), fov.getValue(), renderDistance.getValue(), enableVsync.getValue());

            // Always save after loading to add any missing properties with defaults
            save(filepath);

            return true;

        } catch (const std::exception& e) {
            spdlog::warn("Failed to parse settings file: {}", e.what());
            return false;
        }
    }

    /**
     * Save settings to file. Returns true if successful.
     */
    bool save(const std::string& filepath = "settings.json") const {
        try {
            std::ofstream file(filepath);
            if (!file.is_open()) {
                spdlog::error("Failed to open settings file for writing: {}", filepath);
                return false;
            }

            // Helper lambdas for JSON serialization
            auto writeField = [&](const std::string& key, auto value, bool last = false) {
                file << "  \"" << key << "\": " << value << (last ? "\n" : ",\n");
            };

            auto writeBool = [&](const std::string& key, bool value, bool last = false) {
                file << "  \"" << key << "\": " << (value ? "true" : "false") << (last ? "\n" : ",\n");
            };

            auto writeString = [&](const std::string& key, const std::string& value, bool last = false) {
                file << "  \"" << key << "\": \"" << value << "\"" << (last ? "\n" : ",\n");
            };

            file << "{\n";
            writeField("version", version.getValue());
            writeField("fov", fov.getValue());
            writeField("renderDistance", renderDistance.getValue());
            writeBool("enableVsync", enableVsync.getValue());
            writeBool("fullscreen", fullscreen.getValue());
            writeField("guiScale", guiScale.getValue());
            writeField("maxFps", maxFps.getValue());
            writeField("mipmapLevels", mipmapLevels.getValue());
            writeField("menuBlurAmount", menuBlurAmount.getValue());
            writeBool("renderClouds", renderClouds.getValue());
            writeField("cloudRange", cloudRange.getValue());
            writeString("soundDevice", soundDevice.getValue());
            writeBool("saveChatDrafts", saveChatDrafts.getValue());
            writeField("mouseSensitivity", mouseSensitivity.getValue());

            // Resource packs array
            file << "  \"resourcePacks\": [";
            for (size_t i = 0; i < resourcePacks.size(); i++) {
                file << "\"" << resourcePacks[i] << "\"";
                if (i < resourcePacks.size() - 1) file << ", ";
            }
            file << "],\n";

            // Keybinds
            file << "  \"keybinds\": {\n";
            size_t count = 0;
            for (const auto& [action, key] : keybinds) {
                file << "    \"" << action << "\": \"" << key << "\"";
                if (++count < keybinds.size()) file << ",";
                file << "\n";
            }
            file << "  }\n";
            file << "}\n";

            file.close();
            spdlog::debug("Saved settings (v{})", version.getValue());
            return true;

        } catch (const std::exception& e) {
            spdlog::error("Failed to save settings: {}", e.what());
            return false;
        }
    }
};

} // namespace VoxelEngine