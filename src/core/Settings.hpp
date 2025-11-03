#pragma once

#include <string>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <spdlog/spdlog.h>

namespace VoxelEngine {

/**
 * Manages persistent game settings
 * Saves to and loads from settings.json with versioning support
 */
class Settings {
public:
    // Settings version for compatibility
    int32_t version = 1;

    // Video settings
    float fov = 70.0f;
    int32_t renderDistance = 8;
    bool enableVsync = true;
    bool fullscreen = false;
    int32_t guiScale = 0;  // 0 for auto, 1-6 for manual
    int32_t maxFps = 260;
    int32_t mipmapLevels = 4;
    int32_t menuBlurAmount = 5;

    // Rendering options
    bool renderClouds = false;
    int32_t cloudRange = 128;

    // Audio
    std::string soundDevice = "";

    // Resources
    std::vector<std::string> resourcePacks = {"vanilla"};

    // Chat
    bool saveChatDrafts = false;

    // Mouse settings (0.0 - 1.0, default 0.5 = 50% like Minecraft)
    float mouseSensitivity = 0.5f;

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
     * Minecraft-style scaling: scale 1 = 240px, scale 2 = 480px, scale 3 = 720px, etc.
     * Returns the largest scale that fits the screen.
     */
    static int32_t calculateAutoGuiScale(uint32_t screenHeight) {
        // Each scale level requires 240 pixels of height
        int32_t maxScale = static_cast<int32_t>(screenHeight / 240);
        // Clamp between 1 and 6
        return std::max(1, std::min(6, maxScale));
    }

    /**
     * Get the effective GUI scale.
     * If guiScale is 0 (Auto), returns the calculated scale for the given screen height.
     * Otherwise returns the manual guiScale setting (1-6).
     */
    int32_t getEffectiveGuiScale(uint32_t screenHeight) const {
        if (guiScale == 0) {  // 0 = Auto
            return calculateAutoGuiScale(screenHeight);
        }
        return std::max(1, std::min(6, guiScale));  // Clamp to valid range
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

            // Parse settings (simple key:value parsing)
            auto parseFloat = [&](const std::string& key, float& value) {
                size_t pos = content.find("\"" + key + "\"");
                if (pos != std::string::npos) {
                    size_t colonPos = content.find(':', pos);
                    if (colonPos != std::string::npos) {
                        size_t numStart = content.find_first_of("0123456789.-", colonPos);
                        if (numStart != std::string::npos) {
                            value = std::stof(content.substr(numStart));
                        }
                    }
                }
            };

            auto parseInt = [&](const std::string& key, int32_t& value) {
                size_t pos = content.find("\"" + key + "\"");
                if (pos != std::string::npos) {
                    size_t colonPos = content.find(':', pos);
                    if (colonPos != std::string::npos) {
                        size_t numStart = content.find_first_of("0123456789-", colonPos);
                        if (numStart != std::string::npos) {
                            value = std::stoi(content.substr(numStart));
                        }
                    }
                }
            };

            auto parseBool = [&](const std::string& key, bool& value) {
                size_t pos = content.find("\"" + key + "\"");
                if (pos != std::string::npos) {
                    size_t colonPos = content.find(':', pos);
                    if (colonPos != std::string::npos) {
                        size_t truePos = content.find("true", colonPos);
                        size_t falsePos = content.find("false", colonPos);
                        if (truePos != std::string::npos && (falsePos == std::string::npos || truePos < falsePos)) {
                            value = true;
                        } else if (falsePos != std::string::npos) {
                            value = false;
                        }
                    }
                }
            };

            // Parse all settings
            parseInt("version", version);
            parseFloat("fov", fov);
            parseInt("renderDistance", renderDistance);
            parseBool("enableVsync", enableVsync);
            parseBool("fullscreen", fullscreen);
            parseInt("guiScale", guiScale);
            parseInt("maxFps", maxFps);
            parseInt("mipmapLevels", mipmapLevels);
            parseInt("menuBlurAmount", menuBlurAmount);
            parseBool("renderClouds", renderClouds);
            parseInt("cloudRange", cloudRange);
            parseBool("saveChatDrafts", saveChatDrafts);
            parseFloat("mouseSensitivity", mouseSensitivity);

            // Parse keybinds
            size_t keybindsPos = content.find("\"keybinds\"");
            if (keybindsPos != std::string::npos) {
                size_t openBrace = content.find('{', keybindsPos);
                size_t closeBrace = content.find('}', openBrace);
                if (openBrace != std::string::npos && closeBrace != std::string::npos) {
                    std::string keybindsBlock = content.substr(openBrace + 1, closeBrace - openBrace - 1);

                    // Parse each keybind line
                    size_t pos = 0;
                    while (pos < keybindsBlock.size()) {
                        size_t keyStart = keybindsBlock.find('"', pos);
                        if (keyStart == std::string::npos) break;

                        size_t keyEnd = keybindsBlock.find('"', keyStart + 1);
                        if (keyEnd == std::string::npos) break;

                        std::string key = keybindsBlock.substr(keyStart + 1, keyEnd - keyStart - 1);

                        size_t valueStart = keybindsBlock.find('"', keyEnd + 1);
                        if (valueStart == std::string::npos) break;

                        size_t valueEnd = keybindsBlock.find('"', valueStart + 1);
                        if (valueEnd == std::string::npos) break;

                        std::string value = keybindsBlock.substr(valueStart + 1, valueEnd - valueStart - 1);

                        keybinds[key] = value;
                        pos = valueEnd + 1;
                    }
                }
            }

            spdlog::info("Loaded settings (v{}): FOV={}, RenderDistance={}, VSync={}",
                        version, fov, renderDistance, enableVsync);

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

            file << "{\n";
            file << "  \"version\": " << version << ",\n";
            file << "  \"fov\": " << fov << ",\n";
            file << "  \"renderDistance\": " << renderDistance << ",\n";
            file << "  \"enableVsync\": " << (enableVsync ? "true" : "false") << ",\n";
            file << "  \"fullscreen\": " << (fullscreen ? "true" : "false") << ",\n";
            file << "  \"guiScale\": " << guiScale << ",\n";
            file << "  \"maxFps\": " << maxFps << ",\n";
            file << "  \"mipmapLevels\": " << mipmapLevels << ",\n";
            file << "  \"menuBlurAmount\": " << menuBlurAmount << ",\n";
            file << "  \"renderClouds\": " << (renderClouds ? "true" : "false") << ",\n";
            file << "  \"cloudRange\": " << cloudRange << ",\n";
            file << "  \"soundDevice\": \"" << soundDevice << "\",\n";
            file << "  \"saveChatDrafts\": " << (saveChatDrafts ? "true" : "false") << ",\n";
            file << "  \"mouseSensitivity\": " << mouseSensitivity << ",\n";

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
            spdlog::debug("Saved settings (v{})", version);
            return true;

        } catch (const std::exception& e) {
            spdlog::error("Failed to save settings: {}", e.what());
            return false;
        }
    }
};

} // namespace VoxelEngine