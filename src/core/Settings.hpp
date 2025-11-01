#pragma once

#include <string>
#include <fstream>
#include <spdlog/spdlog.h>

namespace VoxelEngine {

/**
 * Manages persistent game settings (FOV, render distance, etc.)
 * Saves to and loads from settings.json
 */
class Settings {
public:
    // Default values
    float fov = 70.0f;
    int32_t renderDistance = 8;

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

            // Simple manual parsing for just two values
            size_t fovPos = content.find("\"fov\"");
            size_t renderDistPos = content.find("\"renderDistance\"");

            if (fovPos != std::string::npos) {
                size_t colonPos = content.find(':', fovPos);
                if (colonPos != std::string::npos) {
                    size_t numStart = content.find_first_of("0123456789.-", colonPos);
                    if (numStart != std::string::npos) {
                        fov = std::stof(content.substr(numStart));
                    }
                }
            }

            if (renderDistPos != std::string::npos) {
                size_t colonPos = content.find(':', renderDistPos);
                if (colonPos != std::string::npos) {
                    size_t numStart = content.find_first_of("0123456789", colonPos);
                    if (numStart != std::string::npos) {
                        renderDistance = std::stoi(content.substr(numStart));
                    }
                }
            }

            spdlog::info("Loaded settings: FOV={}, RenderDistance={}", fov, renderDistance);
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
            file << "  \"fov\": " << fov << ",\n";
            file << "  \"renderDistance\": " << renderDistance << "\n";
            file << "}\n";

            file.close();
            spdlog::debug("Saved settings: FOV={}, RenderDistance={}", fov, renderDistance);
            return true;

        } catch (const std::exception& e) {
            spdlog::error("Failed to save settings: {}", e.what());
            return false;
        }
    }
};

} // namespace VoxelEngine