#pragma once

// Prevent Windows.h from defining min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Include miniaudio header only (implementation is in AudioManager.cpp)
#include "miniaudio.h"

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <random>
#include <fstream>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <simdjson.h>
#include <magic_enum/magic_enum.hpp>

// Configure magic_enum to support ma_result's range (-403 to 0)
template <>
struct magic_enum::customize::enum_range<ma_result> {
    static constexpr int min = -403;
    static constexpr int max = 0;
};

// Configure magic_enum to support ma_format's range (0 to ma_format_count)
template <>
struct magic_enum::customize::enum_range<ma_format> {
    static constexpr int min = 0;
    static constexpr int max = ma_format_count;
};

// fmt formatter for ma_result
template <>
struct fmt::formatter<ma_result> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const ma_result& result, FormatContext& ctx) const {
        auto name = magic_enum::enum_name(result);
        if (!name.empty()) {
            return fmt::format_to(ctx.out(), "{}", name);
        }
        return fmt::format_to(ctx.out(), "UNKNOWN({})", static_cast<int>(result));
    }
};

// fmt formatter for ma_format
template <>
struct fmt::formatter<ma_format> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const ma_format& format, FormatContext& ctx) const {
        auto name = magic_enum::enum_name(format);
        if (!name.empty()) {
            return fmt::format_to(ctx.out(), "{}", name);
        }
        return fmt::format_to(ctx.out(), "UNKNOWN({})", static_cast<int>(format));
    }
};

namespace FarHorizon {

class Sound {
public:
    Sound() = default;
    ~Sound() {
        if (initialized_) {
            ma_sound_uninit(&sound_);
        }
    }

    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;

    Sound(Sound&& other) noexcept
        : sound_(other.sound_)
        , initialized_(other.initialized_) {
        other.initialized_ = false;
    }

    Sound& operator=(Sound&& other) noexcept {
        if (this != &other) {
            if (initialized_) {
                ma_sound_uninit(&sound_);
            }
            sound_ = other.sound_;
            initialized_ = other.initialized_;
            other.initialized_ = false;
        }
        return *this;
    }

    bool init(ma_engine* engine, const std::string& filepath) {
        std::filesystem::path normalizedPath(filepath);
        std::string pathStr = normalizedPath.make_preferred().string();

        if (!std::filesystem::exists(pathStr)) {
            spdlog::error("Sound file does not exist: {}", pathStr);
            spdlog::error("  Absolute path: {}", std::filesystem::absolute(pathStr).string());
            spdlog::error("  Current working directory: {}", std::filesystem::current_path().string());
            return false;
        }

        std::filesystem::path absolutePath = std::filesystem::absolute(pathStr);
        spdlog::debug("Attempting to load sound from: {}", absolutePath.string());

        ma_decoder decoder;
        ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 2, 48000);
        ma_result decodeResult = ma_decoder_init_file(absolutePath.string().c_str(), &decoderConfig, &decoder);
        if (decodeResult != MA_SUCCESS) {
            spdlog::error("Failed to initialize decoder for: {} (error: {})", absolutePath.string(), magic_enum::enum_name(decodeResult));
            spdlog::error("  This likely means the Vorbis decoder is not available or the file format is not supported");

            decodeResult = ma_decoder_init_file(absolutePath.string().c_str(), nullptr, &decoder);
            if (decodeResult != MA_SUCCESS) {
                spdlog::error("  Auto-detection also failed (error: {})", magic_enum::enum_name(decodeResult));
                return false;
            }
            spdlog::info("  Auto-detection succeeded! Format: {}, channels: {}, sample rate: {}",
                         magic_enum::enum_name(decoder.outputFormat), decoder.outputChannels, decoder.outputSampleRate);
            ma_decoder_uninit(&decoder);
        } else {
            spdlog::debug("Decoder test successful");
            ma_decoder_uninit(&decoder);
        }

        ma_result result = ma_sound_init_from_file(engine, absolutePath.string().c_str(), 0, nullptr, nullptr, &sound_);
        if (result != MA_SUCCESS) {
            spdlog::error("Failed to load sound: {} (error: {})", absolutePath.string(), magic_enum::enum_name(result));
            return false;
        }
        initialized_ = true;
        spdlog::debug("Successfully loaded sound: {}", absolutePath.string());
        return true;
    }

    bool initFromFile(ma_engine* engine, const std::filesystem::path& filepath, ma_uint32 flags) {
        ma_result result = ma_sound_init_from_file(engine, filepath.string().c_str(), flags, nullptr, nullptr, &sound_);
        if (result != MA_SUCCESS) {
            return false;
        }
        initialized_ = true;
        return true;
    }

    void play() {
        if (initialized_) {
            ma_sound_start(&sound_);
        }
    }

    void stop() {
        if (initialized_) {
            ma_sound_stop(&sound_);
        }
    }

    void setVolume(float volume) {
        if (initialized_) {
            ma_sound_set_volume(&sound_, volume);
        }
    }

    void setPitch(float pitch) {
        if (initialized_) {
            ma_sound_set_pitch(&sound_, pitch);
        }
    }

    void setLooping(bool loop) {
        if (initialized_) {
            ma_sound_set_looping(&sound_, loop ? MA_TRUE : MA_FALSE);
        }
    }

    bool isPlaying() const {
        return initialized_ && ma_sound_is_playing(&sound_);
    }

    bool atEnd() const {
        return initialized_ && ma_sound_at_end(&sound_);
    }

    void seekToStart() {
        if (initialized_) {
            ma_sound_seek_to_pcm_frame(&sound_, 0);
        }
    }

private:
    ma_sound sound_{};
    bool initialized_ = false;
};

class AudioManager {
public:
    AudioManager() = default;
    ~AudioManager() {
        cleanup();
    }

    // Prevent copying
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    bool init(ma_device_id* pDeviceId = nullptr) {
        ma_engine_config config = ma_engine_config_init();

        if (pDeviceId != nullptr) {
            config.pPlaybackDeviceID = pDeviceId;
        }

        ma_result result = ma_engine_init(&config, &engine_);
        if (result != MA_SUCCESS) {
            spdlog::error("Failed to initialize audio engine (error: {})", magic_enum::enum_name(result));
            return false;
        }
        initialized_ = true;
        spdlog::info("AudioManager initialized successfully");
        return true;
    }

    void cleanup() {
        if (initialized_) {
            sounds_.clear();
            activeSounds_.clear();
            ma_engine_uninit(&engine_);
            initialized_ = false;
            spdlog::info("AudioManager cleaned up");
        }
    }

    // Load a sound and store it with a given name
    bool loadSound(const std::string& name, const std::string& filepath) {
        if (!initialized_) {
            spdlog::error("Cannot load sound: AudioManager not initialized");
            return false;
        }

        auto sound = std::make_unique<Sound>();
        if (!sound->init(&engine_, filepath)) {
            return false;
        }

        sounds_[name] = std::move(sound);
        spdlog::info("Loaded sound: {} from {}", name, filepath);
        return true;
    }

    // Play a loaded sound
    void playSound(const std::string& name) {
        auto it = sounds_.find(name);
        if (it != sounds_.end()) {
            it->second->play();
        } else {
            spdlog::warn("Sound not found: {}", name);
        }
    }

    // Stop a playing sound
    void stopSound(const std::string& name) {
        auto it = sounds_.find(name);
        if (it != sounds_.end()) {
            it->second->stop();
        }
    }

    // Set volume for a specific sound (0.0 to 1.0)
    void setSoundVolume(const std::string& name, float volume) {
        auto it = sounds_.find(name);
        if (it != sounds_.end()) {
            it->second->setVolume(volume);
        }
    }

    // Set looping for a specific sound
    void setSoundLooping(const std::string& name, bool loop) {
        auto it = sounds_.find(name);
        if (it != sounds_.end()) {
            it->second->setLooping(loop);
        }
    }

    // Set master volume (0.0 to 1.0)
    void setMasterVolume(float volume) {
        if (initialized_) {
            ma_engine_set_volume(&engine_, volume);
        }
    }

    // Play a one-shot sound without storing it
    void playOneShot(const std::string& filepath, float volume = 1.0f) {
        if (initialized_) {
            ma_engine_play_sound(&engine_, filepath.c_str(), nullptr);
        }
    }

    // Check if a sound is playing
    bool isSoundPlaying(const std::string& name) const {
        auto it = sounds_.find(name);
        if (it != sounds_.end()) {
            return it->second->isPlaying();
        }
        return false;
    }

    // Load sounds from sounds.json
    bool loadSoundsFromJson(const std::string& jsonPath, const std::string& soundsBasePath = "assets/minecraft/sounds/") {
        if (!initialized_) {
            spdlog::error("Cannot load sounds: AudioManager not initialized");
            return false;
        }

        // Store the base path for later use in playSoundEvent
        soundsBasePath_ = soundsBasePath;

        // Read JSON file
        std::ifstream file(jsonPath);
        if (!file.is_open()) {
            spdlog::error("Failed to open sounds.json: {}", jsonPath);
            return false;
        }

        std::string jsonContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // Parse JSON using simdjson
        simdjson::dom::parser parser;
        simdjson::dom::element doc;
        auto error = parser.parse(jsonContent).get(doc);
        if (error) {
            spdlog::error("Failed to parse sounds.json: {}", simdjson::error_message(error));
            return false;
        }

        // Iterate through sound events
        for (auto [key, value] : doc.get_object()) {
            std::string eventName(key);

            // Get the sounds array for this event
            auto soundsArray = value["sounds"];
            if (soundsArray.error()) {
                spdlog::warn("No sounds array found for event: {}", eventName);
                continue;
            }

            std::vector<std::string> soundPaths;
            for (auto soundElement : soundsArray.get_array()) {
                std::string_view soundPath = soundElement.get_string();
                soundPaths.emplace_back(soundPath);
            }

            if (!soundPaths.empty()) {
                soundEvents_[eventName] = soundPaths;
                spdlog::info("Registered sound event '{}' with {} variations", eventName, soundPaths.size());
            }
        }

        spdlog::info("Loaded {} sound events from {}", soundEvents_.size(), jsonPath);
        return true;
    }

    void playSoundEvent(const std::string& eventName, float volume = 1.0f, float pitch = 1.0f) {
        auto it = soundEvents_.find(eventName);
        if (it == soundEvents_.end()) {
            spdlog::warn("Sound event not found: {}", eventName);
            return;
        }

        const auto& variations = it->second;
        if (variations.empty()) {
            return;
        }

        std::uniform_int_distribution<size_t> dist(0, variations.size() - 1);
        size_t index = dist(rng_);

        std::string soundPath = soundsBasePath_ + variations[index] + ".ogg";
        std::filesystem::path normalizedPath(soundPath);
        std::filesystem::path absolutePath = std::filesystem::absolute(normalizedPath.make_preferred());

        // spdlog::debug("Playing sound event '{}' from: {}", eventName, absolutePath.string());

        if (initialized_) {
            cleanupFinishedSounds();

            auto sound = std::make_unique<Sound>();
            ma_uint32 flags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION;

            if (sound->initFromFile(&engine_, absolutePath, flags)) {
                // spdlog::debug("Sound initialized successfully, setting volume={} pitch={}", volume, pitch);

                sound->setVolume(volume);
                sound->setPitch(pitch);
                sound->play();

                spdlog::debug("Sound started successfully");
                activeSounds_.push_back(std::move(sound));
            } else {
                spdlog::warn("Failed to initialize sound event '{}'", eventName);
            }
        }
    }

    void cleanupFinishedSounds() {
        auto it = activeSounds_.begin();
        while (it != activeSounds_.end()) {
            if (!(*it)->isPlaying() && (*it)->atEnd()) {
                // spdlog::debug("Cleaning up finished sound");
                it = activeSounds_.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool isInitialized() const { return initialized_; }

    // Get list of available audio output devices
    std::vector<std::string> getAvailableDevices() {
        std::vector<std::string> deviceNames;

        ma_context context;
        if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS) {
            spdlog::error("Failed to initialize context for device enumeration");
            return deviceNames;
        }

        ma_device_info* pPlaybackInfos;
        ma_uint32 playbackCount;

        if (ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, nullptr, nullptr) != MA_SUCCESS) {
            spdlog::error("Failed to get device list");
            ma_context_uninit(&context);
            return deviceNames;
        }

        // Add "Default" as first option
        deviceNames.push_back("Default");

        // Add all available devices
        for (ma_uint32 i = 0; i < playbackCount; i++) {
            deviceNames.push_back(std::string(pPlaybackInfos[i].name));
        }

        ma_context_uninit(&context);
        spdlog::info("Found {} audio output devices", deviceNames.size());
        return deviceNames;
    }

    // Get current device name
    std::string getCurrentDeviceName() {
        if (!initialized_) {
            return "Default";
        }

        ma_device* pDevice = ma_engine_get_device(&engine_);
        if (pDevice == nullptr) {
            return "Default";
        }

        return std::string(pDevice->playback.name);
    }

    // Switch to a different audio device (requires reinitialization)
    bool switchDevice(const std::string& deviceName) {
        if (!initialized_) {
            spdlog::error("Cannot switch device: AudioManager not initialized");
            return false;
        }

        spdlog::info("Switching audio device to: {}", deviceName);

        // Store current volume
        float currentVolume = ma_engine_get_volume(&engine_);

        // Clean up current engine
        sounds_.clear();
        activeSounds_.clear();
        ma_engine_uninit(&engine_);
        initialized_ = false;

        // Reinitialize with new device
        ma_result result;

        if (deviceName == "Default") {
            // Use default device
            result = ma_engine_init(nullptr, &engine_);
        } else {
            // Find and use specific device
            ma_context context;
            if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS) {
                spdlog::error("Failed to initialize context for device switching");
                // Try to recover with default device
                result = ma_engine_init(nullptr, &engine_);
                if (result == MA_SUCCESS) {
                    initialized_ = true;
                    ma_engine_set_volume(&engine_, currentVolume);
                }
                return false;
            }

            ma_device_info* pPlaybackInfos;
            ma_uint32 playbackCount;

            if (ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, nullptr, nullptr) != MA_SUCCESS) {
                spdlog::error("Failed to get device list for switching");
                ma_context_uninit(&context);
                // Try to recover with default device
                result = ma_engine_init(nullptr, &engine_);
                if (result == MA_SUCCESS) {
                    initialized_ = true;
                    ma_engine_set_volume(&engine_, currentVolume);
                }
                return false;
            }

            // Find the device ID
            ma_device_id* pDeviceId = nullptr;
            for (ma_uint32 i = 0; i < playbackCount; i++) {
                if (deviceName == std::string(pPlaybackInfos[i].name)) {
                    pDeviceId = &pPlaybackInfos[i].id;
                    break;
                }
            }

            if (pDeviceId == nullptr) {
                spdlog::warn("Device '{}' not found, using default", deviceName);
                ma_context_uninit(&context);
                result = ma_engine_init(nullptr, &engine_);
            } else {
                // Create engine config with the specific device
                ma_engine_config engineConfig = ma_engine_config_init();
                engineConfig.pPlaybackDeviceID = pDeviceId;

                result = ma_engine_init(&engineConfig, &engine_);
                ma_context_uninit(&context);
            }
        }

        if (result != MA_SUCCESS) {
            spdlog::error("Failed to reinitialize audio engine (error: {})", magic_enum::enum_name(result));
            // Try to recover with default device
            result = ma_engine_init(nullptr, &engine_);
            if (result != MA_SUCCESS) {
                spdlog::error("Failed to recover audio engine (error: {})", magic_enum::enum_name(result));
                return false;
            }
        }

        initialized_ = true;

        // Restore volume
        ma_engine_set_volume(&engine_, currentVolume);

        // Reload sound events if they were previously loaded
        if (!soundEvents_.empty() && !soundsBasePath_.empty()) {
            // Store the events temporarily
            auto tempEvents = soundEvents_;
            auto tempBasePath = soundsBasePath_;

            // Reload (this will repopulate soundEvents_)
            loadSoundsFromJson("assets/minecraft/sounds.json", tempBasePath);
        }

        spdlog::info("Successfully switched to audio device: {}", getCurrentDeviceName());
        return true;
    }

private:
    ma_engine engine_{};
    bool initialized_ = false;
    std::unordered_map<std::string, std::unique_ptr<Sound>> sounds_;
    std::unordered_map<std::string, std::vector<std::string>> soundEvents_;
    std::string soundsBasePath_;
    std::mt19937 rng_{std::random_device{}()};
    std::vector<std::unique_ptr<Sound>> activeSounds_;
};

} // namespace FarHorizon