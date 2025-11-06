#pragma once
#include "ChunkGeneratorSettings.hpp"
#include "DensityFunctionTypes.hpp"
#include "../Chunk.hpp"
#include "../BlockRegistry.hpp"
#include <filesystem>

namespace VoxelEngine {

class WorldGenerator {
public:
    WorldGenerator(int seed) : m_seed(seed) {
        DensityFunctionTypes::registerTypes();
    }

    bool loadFromDirectory(const std::string& worldgenDir) {
        std::filesystem::path basePath(worldgenDir);

        if (!loadNoiseParameters(basePath / "noise")) {
            spdlog::error("Failed to load noise parameters");
            return false;
        }

        if (!loadDensityFunctions(basePath / "density_function")) {
            spdlog::error("Failed to load density functions");
            return false;
        }

        if (!loadChunkGeneratorSettings(basePath / "settings.json")) {
            spdlog::error("Failed to load chunk generator settings");
            return false;
        }

        m_noiseRegistry.freeze();
        m_densityFunctionRegistry.freeze();

        spdlog::info("World generator initialized successfully");
        return true;
    }

    void generateChunk(Chunk& chunk, const ChunkPosition& pos) const {
        if (!m_settings) {
            spdlog::error("Cannot generate chunk: settings not loaded");
            return;
        }

        const auto& shape = m_settings->generationShapeConfig;
        const auto& router = m_settings->noiseRouter;

        int chunkWorldX = pos.x * CHUNK_SIZE;
        int chunkWorldY = pos.y * CHUNK_SIZE;
        int chunkWorldZ = pos.z * CHUNK_SIZE;

        // Batch compute density for entire chunk using SIMD
        constexpr int totalSize = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
        std::vector<float> densityGrid(totalSize);

        router.finalDensity->computeGrid(
            densityGrid.data(),
            CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE,
            static_cast<double>(chunkWorldX),
            static_cast<double>(chunkWorldY),
            static_cast<double>(chunkWorldZ),
            1.0, 1.0, 1.0,
            shape.horizontalSize,
            shape.verticalSize
        );

        // Apply density values to set blocks
        int index = 0;
        for (int localZ = 0; localZ < CHUNK_SIZE; localZ++) {
            for (int localY = 0; localY < CHUNK_SIZE; localY++) {
                for (int localX = 0; localX < CHUNK_SIZE; localX++) {
                    int worldY = chunkWorldY + localY;

                    if (worldY < shape.minY || worldY >= shape.minY + shape.height) {
                        index++;
                        continue;
                    }

                    float density = densityGrid[index++];

                    BlockState state;
                    if (density > 0.0f) {
                        state = m_settings->defaultBlock;
                    } else if (worldY <= m_settings->seaLevel) {
                        state = m_settings->defaultFluid;
                    } else {
                        continue;
                    }

                    chunk.setBlockState(localX, localY, localZ, state);
                }
            }
        }
    }

    const ChunkGeneratorSettings* getSettings() const {
        return m_settings.get();
    }

private:
    int m_seed;
    Registry<NoiseGenerator> m_noiseRegistry;
    Registry<DensityFunction> m_densityFunctionRegistry;
    std::unique_ptr<ChunkGeneratorSettings> m_settings;

    bool loadNoiseParameters(const std::filesystem::path& dir) {
        if (!std::filesystem::exists(dir)) {
            spdlog::warn("Noise parameters directory does not exist: {}", dir.string());
            return false;
        }

        simdjson::ondemand::parser parser;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") {
                continue;
            }

            std::string id = getIdFromPath(dir, entry.path());

            try {
                auto json = simdjson::padded_string::load(entry.path().string());
                simdjson::ondemand::document doc = parser.iterate(json);
                simdjson::ondemand::value val = doc.get_value();

                auto result = NoiseParameters::codec().decode(val);
                if (result.isError()) {
                    spdlog::error("Failed to decode noise {}: {}", id, result.error);
                    continue;
                }

                auto noise = std::make_shared<NoiseGenerator>(
                    NoiseType::SIMPLEX,
                    result.value.value(),
                    m_seed
                );

                m_noiseRegistry.registerEntry(id, noise);

            } catch (const std::exception& e) {
                spdlog::error("Failed to load noise {}: {}", id, e.what());
            }
        }

        return m_noiseRegistry.getAll().size() > 0;
    }

    bool loadDensityFunctions(const std::filesystem::path& dir) {
        if (!std::filesystem::exists(dir)) {
            spdlog::warn("Density functions directory does not exist: {}", dir.string());
            return false;
        }

        simdjson::ondemand::parser parser;
        auto codec = DensityFunctionTypes::codec(m_noiseRegistry, m_densityFunctionRegistry);

        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") {
                continue;
            }

            std::string id = getIdFromPath(dir, entry.path());

            try {
                auto json = simdjson::padded_string::load(entry.path().string());
                simdjson::ondemand::document doc = parser.iterate(json);
                simdjson::ondemand::value val = doc.get_value();

                auto result = codec.decode(val);
                if (result.isError()) {
                    spdlog::error("Failed to decode density function {}: {}", id, result.error);
                    continue;
                }

                m_densityFunctionRegistry.registerEntry(id, result.value.value());

            } catch (const std::exception& e) {
                spdlog::error("Failed to load density function {}: {}", id, e.what());
            }
        }

        return true;
    }

    bool loadChunkGeneratorSettings(const std::filesystem::path& file) {
        if (!std::filesystem::exists(file)) {
            spdlog::error("Settings file does not exist: {}", file.string());
            return false;
        }

        try {
            simdjson::ondemand::parser parser;
            auto json = simdjson::padded_string::load(file.string());
            simdjson::ondemand::document doc = parser.iterate(json);
            simdjson::ondemand::value val = doc.get_value();

            auto codec = ChunkGeneratorSettings::codec(m_noiseRegistry, m_densityFunctionRegistry);
            auto result = codec.decode(val);

            if (result.isError()) {
                spdlog::error("Failed to decode settings: {}", result.error);
                return false;
            }

            m_settings = std::make_unique<ChunkGeneratorSettings>(std::move(result.value.value()));

            BlockState stone = BlockRegistry::STONE->getDefaultState();
            BlockState air = BlockRegistry::AIR->getDefaultState();
            m_settings->defaultBlock = stone;
            m_settings->defaultFluid = air;

            return true;

        } catch (const std::exception& e) {
            spdlog::error("Failed to load settings: {}", e.what());
            return false;
        }
    }

    static std::string getIdFromPath(const std::filesystem::path& baseDir, const std::filesystem::path& filePath) {
        auto relativePath = std::filesystem::relative(filePath, baseDir);
        std::string id = relativePath.string();

        std::replace(id.begin(), id.end(), '\\', '/');

        if (id.ends_with(".json")) {
            id = id.substr(0, id.length() - 5);
        }

        return id;
    }
};

} // namespace VoxelEngine