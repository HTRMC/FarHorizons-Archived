#pragma once
#include "NoiseRouter.hpp"
#include "../BlockState.hpp"
#include <string>

namespace VoxelEngine {

struct GenerationShapeConfig {
    int minY;
    int height;
    int horizontalSize;
    int verticalSize;

    GenerationShapeConfig()
        : minY(-64), height(384), horizontalSize(1), verticalSize(2) {}

    static Codec<GenerationShapeConfig> codec() {
        return Codec<GenerationShapeConfig>([](simdjson::ondemand::value json) -> DecodeResult<GenerationShapeConfig> {
            simdjson::ondemand::object obj;
            auto error = json.get_object().get(obj);
            if (error) {
                return DecodeResult<GenerationShapeConfig>::failure("Expected object");
            }

            GenerationShapeConfig config;

            auto minYField = optionalField("minY", Codecs::INT32(), -64);
            auto heightField = optionalField("height", Codecs::INT32(), 384);
            auto horizontalSizeField = optionalField("horizontalSize", Codecs::INT32(), 1);
            auto verticalSizeField = optionalField("verticalSize", Codecs::INT32(), 2);

            auto minY = minYField.decode(obj);
            if (minY.isError()) {
                return DecodeResult<GenerationShapeConfig>::failure(minY.error);
            }
            config.minY = minY.value.value();

            obj.reset();
            auto height = heightField.decode(obj);
            if (height.isError()) {
                return DecodeResult<GenerationShapeConfig>::failure(height.error);
            }
            config.height = height.value.value();

            obj.reset();
            auto horizontalSize = horizontalSizeField.decode(obj);
            if (horizontalSize.isError()) {
                return DecodeResult<GenerationShapeConfig>::failure(horizontalSize.error);
            }
            config.horizontalSize = horizontalSize.value.value();

            obj.reset();
            auto verticalSize = verticalSizeField.decode(obj);
            if (verticalSize.isError()) {
                return DecodeResult<GenerationShapeConfig>::failure(verticalSize.error);
            }
            config.verticalSize = verticalSize.value.value();

            return DecodeResult<GenerationShapeConfig>::success(std::move(config));
        });
    }
};

class ChunkGeneratorSettings {
public:
    GenerationShapeConfig generationShapeConfig;
    NoiseRouter noiseRouter;
    BlockState defaultBlock;
    BlockState defaultFluid;
    int seaLevel;

    ChunkGeneratorSettings()
        : seaLevel(63) {}

    static Codec<ChunkGeneratorSettings> codec(
        const Registry<NoiseGenerator>& noiseRegistry,
        const Registry<DensityFunction>& densityRegistry
    ) {
        return Codec<ChunkGeneratorSettings>([&noiseRegistry, &densityRegistry](
            simdjson::ondemand::value json
        ) -> DecodeResult<ChunkGeneratorSettings> {
            simdjson::ondemand::object obj;
            auto error = json.get_object().get(obj);
            if (error) {
                return DecodeResult<ChunkGeneratorSettings>::failure("Expected object");
            }

            ChunkGeneratorSettings settings;

            auto shapeField = optionalField("generationShapeConfig", GenerationShapeConfig::codec(), GenerationShapeConfig());
            auto noiseRouterField = field("noiseRouter", NoiseRouter::codec(noiseRegistry, densityRegistry));
            auto seaLevelField = optionalField("seaLevel", Codecs::INT32(), 63);

            auto shape = shapeField.decode(obj);
            if (shape.isError()) {
                return DecodeResult<ChunkGeneratorSettings>::failure(shape.error);
            }
            settings.generationShapeConfig = shape.value.value();

            obj.reset();
            auto router = noiseRouterField.decode(obj);
            if (router.isError()) {
                return DecodeResult<ChunkGeneratorSettings>::failure(router.error);
            }
            settings.noiseRouter = router.value.value();

            obj.reset();
            auto seaLevel = seaLevelField.decode(obj);
            if (seaLevel.isError()) {
                return DecodeResult<ChunkGeneratorSettings>::failure(seaLevel.error);
            }
            settings.seaLevel = seaLevel.value.value();

            return DecodeResult<ChunkGeneratorSettings>::success(std::move(settings));
        });
    }
};

} // namespace VoxelEngine