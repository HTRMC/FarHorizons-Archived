#pragma once
#include "Codec.hpp"
#include <memory>
#include <unordered_map>
#include <string>
#include <filesystem>
#include <spdlog/spdlog.h>

namespace VoxelEngine {

template<typename T>
class Registry {
public:
    void registerEntry(const std::string& id, std::shared_ptr<T> entry) {
        if (m_frozen) {
            spdlog::error("Cannot register to frozen registry: {}", id);
            return;
        }

        if (m_entries.contains(id)) {
            spdlog::warn("Overwriting registry entry: {}", id);
        }

        m_entries[id] = std::move(entry);
        spdlog::debug("Registered: {}", id);
    }

    std::shared_ptr<T> get(const std::string& id) const {
        auto it = m_entries.find(id);
        if (it == m_entries.end()) {
            return nullptr;
        }
        return it->second;
    }

    bool contains(const std::string& id) const {
        return m_entries.contains(id);
    }

    const std::unordered_map<std::string, std::shared_ptr<T>>& getAll() const {
        return m_entries;
    }

    void freeze() {
        m_frozen = true;
        spdlog::info("Registry frozen with {} entries", m_entries.size());
    }

    bool isFrozen() const {
        return m_frozen;
    }

    void clear() {
        if (m_frozen) {
            spdlog::error("Cannot clear frozen registry");
            return;
        }
        m_entries.clear();
    }

    Codec<std::shared_ptr<T>> referenceCodec() const {
        return Codec<std::shared_ptr<T>>([this](simdjson::ondemand::value json) -> DecodeResult<std::shared_ptr<T>> {
            std::string_view id_view;
            auto error = json.get_string().get(id_view);
            if (error) {
                return DecodeResult<std::shared_ptr<T>>::failure("Expected string for registry reference");
            }

            std::string id(id_view);
            auto entry = get(id);
            if (!entry) {
                return DecodeResult<std::shared_ptr<T>>::failure("Registry entry not found: " + id);
            }

            return DecodeResult<std::shared_ptr<T>>::success(entry);
        });
    }

    Codec<std::shared_ptr<T>> codec(const Codec<T>& entryCodec) const {
        return Codec<std::shared_ptr<T>>([this, entryCodec](simdjson::ondemand::value json) -> DecodeResult<std::shared_ptr<T>> {
            simdjson::ondemand::json_type type;
            auto type_error = json.type().get(type);
            if (type_error) {
                return DecodeResult<std::shared_ptr<T>>::failure("Failed to determine JSON type");
            }

            if (type == simdjson::ondemand::json_type::string) {
                return referenceCodec().decode(json);
            }

            auto decoded = entryCodec.decode(json);
            if (decoded.isError()) {
                return DecodeResult<std::shared_ptr<T>>::failure(decoded.error);
            }

            return DecodeResult<std::shared_ptr<T>>::success(
                std::make_shared<T>(std::move(decoded.value.value()))
            );
        });
    }

private:
    std::unordered_map<std::string, std::shared_ptr<T>> m_entries;
    bool m_frozen = false;
};

template<typename T>
class RegistryLoader {
public:
    static bool loadFromDirectory(Registry<T>& registry,
                                   const std::string& directory,
                                   const Codec<T>& codec) {
        spdlog::info("Loading registry from: {}", directory);

        if (!std::filesystem::exists(directory)) {
            spdlog::warn("Directory does not exist: {}", directory);
            return false;
        }

        simdjson::ondemand::parser parser;
        size_t loadedCount = 0;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") {
                continue;
            }

            std::string id = getIdFromPath(directory, entry.path());

            try {
                auto json = simdjson::padded_string::load(entry.path().string());
                simdjson::ondemand::document doc = parser.iterate(json);
                simdjson::ondemand::value val = doc.get_value();

                auto result = codec.decode(val);
                if (result.isError()) {
                    spdlog::error("Failed to decode {}: {}", id, result.error);
                    continue;
                }

                registry.registerEntry(id, std::make_shared<T>(std::move(result.value.value())));
                loadedCount++;

            } catch (const std::exception& e) {
                spdlog::error("Failed to load {}: {}", id, e.what());
            }
        }

        spdlog::info("Loaded {} entries", loadedCount);
        return loadedCount > 0;
    }

private:
    static std::string getIdFromPath(const std::string& baseDir, const std::filesystem::path& filePath) {
        auto relativePath = std::filesystem::relative(filePath, baseDir);
        std::string id = relativePath.string();

        std::replace(id.begin(), id.end(), '\\', '/');

        if (id.ends_with(".json")) {
            id = id.substr(0, id.length() - 5);
        }

        // Strip directory prefix (e.g., "density_function/continents" -> "continents")
        size_t lastSlash = id.find_last_of('/');
        if (lastSlash != std::string::npos) {
            id = id.substr(lastSlash + 1);
        }

        return id;
    }
};

} // namespace VoxelEngine