#pragma once
#include "DensityFunction.hpp"
#include "DensityFunctionTypes.hpp"
#include <memory>

namespace VoxelEngine {

class NoiseRouter {
public:
    std::shared_ptr<DensityFunction> continents;
    std::shared_ptr<DensityFunction> erosion;
    std::shared_ptr<DensityFunction> ridges;
    std::shared_ptr<DensityFunction> temperature;
    std::shared_ptr<DensityFunction> vegetation;
    std::shared_ptr<DensityFunction> depth;
    std::shared_ptr<DensityFunction> finalDensity;

    static Codec<NoiseRouter> codec(
        const Registry<INoiseSampler>& noiseRegistry,
        const Registry<DensityFunction>& densityRegistry
    ) {
        return Codec<NoiseRouter>([&noiseRegistry, &densityRegistry](
            simdjson::ondemand::value json
        ) -> DecodeResult<NoiseRouter> {
            simdjson::ondemand::object obj;
            auto error = json.get_object().get(obj);
            if (error) {
                return DecodeResult<NoiseRouter>::failure("Expected object");
            }

            auto dfCodec = DensityFunctionTypes::codec(noiseRegistry, densityRegistry);

            auto continentsField = field("continents", dfCodec);
            auto erosionField = field("erosion", dfCodec);
            auto ridgesField = field("ridges", dfCodec);
            auto temperatureField = field("temperature", dfCodec);
            auto vegetationField = field("vegetation", dfCodec);
            auto depthField = field("depth", dfCodec);
            auto finalDensityField = field("finalDensity", dfCodec);

            NoiseRouter router;

            auto continentsResult = continentsField.decode(obj);
            if (continentsResult.isError()) {
                return DecodeResult<NoiseRouter>::failure(continentsResult.error);
            }
            router.continents = continentsResult.value.value();

            obj.reset();
            auto erosionResult = erosionField.decode(obj);
            if (erosionResult.isError()) {
                return DecodeResult<NoiseRouter>::failure(erosionResult.error);
            }
            router.erosion = erosionResult.value.value();

            obj.reset();
            auto ridgesResult = ridgesField.decode(obj);
            if (ridgesResult.isError()) {
                return DecodeResult<NoiseRouter>::failure(ridgesResult.error);
            }
            router.ridges = ridgesResult.value.value();

            obj.reset();
            auto temperatureResult = temperatureField.decode(obj);
            if (temperatureResult.isError()) {
                return DecodeResult<NoiseRouter>::failure(temperatureResult.error);
            }
            router.temperature = temperatureResult.value.value();

            obj.reset();
            auto vegetationResult = vegetationField.decode(obj);
            if (vegetationResult.isError()) {
                return DecodeResult<NoiseRouter>::failure(vegetationResult.error);
            }
            router.vegetation = vegetationResult.value.value();

            obj.reset();
            auto depthResult = depthField.decode(obj);
            if (depthResult.isError()) {
                return DecodeResult<NoiseRouter>::failure(depthResult.error);
            }
            router.depth = depthResult.value.value();

            obj.reset();
            auto finalDensityResult = finalDensityField.decode(obj);
            if (finalDensityResult.isError()) {
                return DecodeResult<NoiseRouter>::failure(finalDensityResult.error);
            }
            router.finalDensity = finalDensityResult.value.value();

            return DecodeResult<NoiseRouter>::success(std::move(router));
        });
    }
};

} // namespace VoxelEngine