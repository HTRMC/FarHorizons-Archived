#pragma once

// Prevent Windows.h from defining min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Step 1: Include stb_vorbis HEADER before miniaudio
#define STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"

// Step 2: Define miniaudio implementation
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// Step 3: Include stb_vorbis IMPLEMENTATION after miniaudio
#undef STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"

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

namespace VoxelEngine {

class Sound {
public:
    Sound() = default;
    ~Sound() {
        if (m_initialized) {
            ma_sound_uninit(&m_sound);
        }
    }

    // Prevent copying
    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;

    // Allow moving
    Sound(Sound&& other) noexcept
        : m_sound(other.m_sound)
        , m_initialized(other.m_initialized) {
        other.m_initialized = false;
    }

    Sound& operator=(Sound&& other) noexcept {
        if (this != &other) {
            if (m_initialized) {
                ma_sound_uninit(&m_sound);
            }
            m_sound = other.m_sound;
            m_initialized = other.m_initialized;
            other.m_initialized = false;
        }
        return *this;
    }

    bool init(ma_engine* engine, const std::string& filepath) {
        // Normalize path separators for the platform
        std::filesystem::path normalizedPath(filepath);
        std::string pathStr = normalizedPath.make_preferred().string();

        // Check if file exists first
        if (!std::filesystem::exists(pathStr)) {
            spdlog::error("Sound file does not exist: {}", pathStr);
            spdlog::error("  Absolute path: {}", std::filesystem::absolute(pathStr).string());
            spdlog::error("  Current working directory: {}", std::filesystem::current_path().string());
            return false;
        }

        // Get absolute path for debugging
        std::filesystem::path absolutePath = std::filesystem::absolute(pathStr);
        spdlog::debug("Attempting to load sound from: {}", absolutePath.string());

        // Try to decode manually first to get better error info
        ma_decoder decoder;
        ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 2, 48000);
        ma_result decodeResult = ma_decoder_init_file(absolutePath.string().c_str(), &decoderConfig, &decoder);
        if (decodeResult != MA_SUCCESS) {
            spdlog::error("Failed to initialize decoder for: {} (error: {})", absolutePath.string(), decodeResult);
            spdlog::error("  This likely means the Vorbis decoder is not available or the file format is not supported");

            // Try with NULL config to let miniaudio auto-detect
            decodeResult = ma_decoder_init_file(absolutePath.string().c_str(), nullptr, &decoder);
            if (decodeResult != MA_SUCCESS) {
                spdlog::error("  Auto-detection also failed (error: {})", decodeResult);
                return false;
            }
            spdlog::info("  Auto-detection succeeded! Format: {}, channels: {}, sample rate: {}",
                         decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate);
            ma_decoder_uninit(&decoder);
        } else {
            spdlog::debug("Decoder test successful");
            ma_decoder_uninit(&decoder);
        }

        ma_result result = ma_sound_init_from_file(engine, absolutePath.string().c_str(), 0, nullptr, nullptr, &m_sound);
        if (result != MA_SUCCESS) {
            spdlog::error("Failed to load sound: {} (error: {})", absolutePath.string(), result);
            return false;
        }
        m_initialized = true;
        spdlog::debug("Successfully loaded sound: {}", absolutePath.string());
        return true;
    }

    void play() {
        if (m_initialized) {
            ma_sound_start(&m_sound);
        }
    }

    void stop() {
        if (m_initialized) {
            ma_sound_stop(&m_sound);
        }
    }

    void setVolume(float volume) {
        if (m_initialized) {
            ma_sound_set_volume(&m_sound, volume);
        }
    }

    void setLooping(bool loop) {
        if (m_initialized) {
            ma_sound_set_looping(&m_sound, loop ? MA_TRUE : MA_FALSE);
        }
    }

    bool isPlaying() const {
        return m_initialized && ma_sound_is_playing(&m_sound);
    }

    void seekToStart() {
        if (m_initialized) {
            ma_sound_seek_to_pcm_frame(&m_sound, 0);
        }
    }

private:
    ma_sound m_sound{};
    bool m_initialized = false;
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

        ma_result result = ma_engine_init(&config, &m_engine);
        if (result != MA_SUCCESS) {
            spdlog::error("Failed to initialize audio engine (error: {})", result);
            return false;
        }
        m_initialized = true;
        spdlog::info("AudioManager initialized successfully");
        return true;
    }

    void cleanup() {
        if (m_initialized) {
            m_sounds.clear();
            ma_engine_uninit(&m_engine);
            m_initialized = false;
            spdlog::info("AudioManager cleaned up");
        }
    }

    // Load a sound and store it with a given name
    bool loadSound(const std::string& name, const std::string& filepath) {
        if (!m_initialized) {
            spdlog::error("Cannot load sound: AudioManager not initialized");
            return false;
        }

        auto sound = std::make_unique<Sound>();
        if (!sound->init(&m_engine, filepath)) {
            return false;
        }

        m_sounds[name] = std::move(sound);
        spdlog::info("Loaded sound: {} from {}", name, filepath);
        return true;
    }

    // Play a loaded sound
    void playSound(const std::string& name) {
        auto it = m_sounds.find(name);
        if (it != m_sounds.end()) {
            it->second->play();
        } else {
            spdlog::warn("Sound not found: {}", name);
        }
    }

    // Stop a playing sound
    void stopSound(const std::string& name) {
        auto it = m_sounds.find(name);
        if (it != m_sounds.end()) {
            it->second->stop();
        }
    }

    // Set volume for a specific sound (0.0 to 1.0)
    void setSoundVolume(const std::string& name, float volume) {
        auto it = m_sounds.find(name);
        if (it != m_sounds.end()) {
            it->second->setVolume(volume);
        }
    }

    // Set looping for a specific sound
    void setSoundLooping(const std::string& name, bool loop) {
        auto it = m_sounds.find(name);
        if (it != m_sounds.end()) {
            it->second->setLooping(loop);
        }
    }

    // Set master volume (0.0 to 1.0)
    void setMasterVolume(float volume) {
        if (m_initialized) {
            ma_engine_set_volume(&m_engine, volume);
        }
    }

    // Play a one-shot sound without storing it
    void playOneShot(const std::string& filepath, float volume = 1.0f) {
        if (m_initialized) {
            ma_engine_play_sound(&m_engine, filepath.c_str(), nullptr);
        }
    }

    // Check if a sound is playing
    bool isSoundPlaying(const std::string& name) const {
        auto it = m_sounds.find(name);
        if (it != m_sounds.end()) {
            return it->second->isPlaying();
        }
        return false;
    }

    // Load sounds from sounds.json
    bool loadSoundsFromJson(const std::string& jsonPath, const std::string& soundsBasePath = "assets/minecraft/sounds/") {
        if (!m_initialized) {
            spdlog::error("Cannot load sounds: AudioManager not initialized");
            return false;
        }

        // Store the base path for later use in playSoundEvent
        m_soundsBasePath = soundsBasePath;

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
                m_soundEvents[eventName] = soundPaths;
                spdlog::info("Registered sound event '{}' with {} variations", eventName, soundPaths.size());
            }
        }

        spdlog::info("Loaded {} sound events from {}", m_soundEvents.size(), jsonPath);
        return true;
    }

    // Play a sound event (randomly selects from variations if available)
    // Uses fire-and-forget playback to allow multiple instances to overlap
    // volume: Volume multiplier (default 1.0)
    // pitch: Pitch multiplier (default 1.0)
    void playSoundEvent(const std::string& eventName, float volume = 1.0f, float pitch = 1.0f) {
        auto it = m_soundEvents.find(eventName);
        if (it == m_soundEvents.end()) {
            spdlog::warn("Sound event not found: {}", eventName);
            return;
        }

        const auto& variations = it->second;
        if (variations.empty()) {
            return;
        }

        // Select random variation
        std::uniform_int_distribution<size_t> dist(0, variations.size() - 1);
        size_t index = dist(m_rng);

        // Build the full path to the sound file
        std::string soundPath = m_soundsBasePath + variations[index] + ".ogg";

        // Normalize path separators for the platform
        std::filesystem::path normalizedPath(soundPath);
        std::filesystem::path absolutePath = std::filesystem::absolute(normalizedPath.make_preferred());

        spdlog::debug("Playing sound event '{}' from: {}", eventName, absolutePath.string());

        // Create a fire-and-forget sound with automatic cleanup
        if (m_initialized) {
            // Clean up finished sounds first
            cleanupFinishedSounds();

            ma_sound* pSound = new ma_sound();

            // Use DECODE flag to fully decode the sound (allows pitch shifting)
            // Use NO_SPATIALIZATION since we don't need 3D audio for block sounds
            // Note: Not using ASYNC flag to ensure sound is ready before playback
            ma_uint32 flags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION;

            ma_result result = ma_sound_init_from_file(&m_engine, absolutePath.string().c_str(),
                flags, nullptr, nullptr, pSound);

            if (result == MA_SUCCESS) {
                spdlog::debug("Sound initialized successfully, setting volume={} pitch={}", volume, pitch);

                // Set volume and pitch
                ma_sound_set_volume(pSound, volume);
                ma_sound_set_pitch(pSound, pitch);

                // Start playback
                result = ma_sound_start(pSound);

                if (result != MA_SUCCESS) {
                    spdlog::warn("Failed to start sound event '{}': {}", eventName, result);
                    ma_sound_uninit(pSound);
                    delete pSound;
                } else {
                    spdlog::debug("Sound started successfully");
                    // Track this sound for later cleanup
                    m_activeSounds.push_back(pSound);
                }
            } else {
                spdlog::warn("Failed to initialize sound event '{}': {}", eventName, result);
                delete pSound;
            }
        }
    }

    // Clean up sounds that have finished playing
    void cleanupFinishedSounds() {
        auto it = m_activeSounds.begin();
        while (it != m_activeSounds.end()) {
            ma_sound* pSound = *it;

            // Check if sound has finished playing
            if (!ma_sound_is_playing(pSound) && ma_sound_at_end(pSound)) {
                spdlog::debug("Cleaning up finished sound");
                ma_sound_uninit(pSound);
                delete pSound;
                it = m_activeSounds.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool isInitialized() const { return m_initialized; }

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
        if (!m_initialized) {
            return "Default";
        }

        ma_device* pDevice = ma_engine_get_device(&m_engine);
        if (pDevice == nullptr) {
            return "Default";
        }

        return std::string(pDevice->playback.name);
    }

    // Switch to a different audio device (requires reinitialization)
    bool switchDevice(const std::string& deviceName) {
        if (!m_initialized) {
            spdlog::error("Cannot switch device: AudioManager not initialized");
            return false;
        }

        spdlog::info("Switching audio device to: {}", deviceName);

        // Store current volume
        float currentVolume = ma_engine_get_volume(&m_engine);

        // Clean up current engine
        m_sounds.clear();
        ma_engine_uninit(&m_engine);
        m_initialized = false;

        // Reinitialize with new device
        ma_result result;

        if (deviceName == "Default") {
            // Use default device
            result = ma_engine_init(nullptr, &m_engine);
        } else {
            // Find and use specific device
            ma_context context;
            if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS) {
                spdlog::error("Failed to initialize context for device switching");
                // Try to recover with default device
                result = ma_engine_init(nullptr, &m_engine);
                if (result == MA_SUCCESS) {
                    m_initialized = true;
                    ma_engine_set_volume(&m_engine, currentVolume);
                }
                return false;
            }

            ma_device_info* pPlaybackInfos;
            ma_uint32 playbackCount;

            if (ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, nullptr, nullptr) != MA_SUCCESS) {
                spdlog::error("Failed to get device list for switching");
                ma_context_uninit(&context);
                // Try to recover with default device
                result = ma_engine_init(nullptr, &m_engine);
                if (result == MA_SUCCESS) {
                    m_initialized = true;
                    ma_engine_set_volume(&m_engine, currentVolume);
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
                result = ma_engine_init(nullptr, &m_engine);
            } else {
                // Create engine config with the specific device
                ma_engine_config engineConfig = ma_engine_config_init();
                engineConfig.pPlaybackDeviceID = pDeviceId;

                result = ma_engine_init(&engineConfig, &m_engine);
                ma_context_uninit(&context);
            }
        }

        if (result != MA_SUCCESS) {
            spdlog::error("Failed to reinitialize audio engine (error: {})", result);
            // Try to recover with default device
            result = ma_engine_init(nullptr, &m_engine);
            if (result != MA_SUCCESS) {
                spdlog::error("Failed to recover audio engine (error: {})", result);
                return false;
            }
        }

        m_initialized = true;

        // Restore volume
        ma_engine_set_volume(&m_engine, currentVolume);

        // Reload sound events if they were previously loaded
        if (!m_soundEvents.empty() && !m_soundsBasePath.empty()) {
            // Store the events temporarily
            auto tempEvents = m_soundEvents;
            auto tempBasePath = m_soundsBasePath;

            // Reload (this will repopulate m_soundEvents)
            loadSoundsFromJson("assets/minecraft/sounds.json", tempBasePath);
        }

        spdlog::info("Successfully switched to audio device: {}", getCurrentDeviceName());
        return true;
    }

private:
    ma_engine m_engine{};
    bool m_initialized = false;
    std::unordered_map<std::string, std::unique_ptr<Sound>> m_sounds;
    std::unordered_map<std::string, std::vector<std::string>> m_soundEvents;  // event name -> sound paths
    std::string m_soundsBasePath;  // Base path for sound files
    std::mt19937 m_rng{std::random_device{}()};  // Random number generator for variation selection
    std::vector<ma_sound*> m_activeSounds;  // Track active fire-and-forget sounds for cleanup
};

} // namespace VoxelEngine