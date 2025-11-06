#pragma once
#include "DensityFunction.hpp"
#include "Registry.hpp"
#include <unordered_map>
#include <functional>

namespace VoxelEngine {

class DensityFunctionTypes {
public:
    using Parser = std::function<DecodeResult<std::shared_ptr<DensityFunction>>(
        simdjson::ondemand::value,
        const Registry<INoiseSampler>&,
        const Registry<DensityFunction>&
    )>;

    static void registerTypes() {
        registerType("constant", parseConstant);
        registerType("y_clamped_gradient", parseYClampedGradient);
        registerType("noise", parseNoise);
        registerType("interpolated", parseInterpolated);
        registerType("add", parseAdd);
        registerType("mul", parseMul);
        registerType("min", parseMin);
        registerType("max", parseMax);
        registerType("clamp", parseClamp);
        registerType("abs", parseAbs);
        registerType("square", parseSquare);
        registerType("quarter_negative", parseQuarterNegative);
        registerType("lerp", parseLerp);
        registerType("spline", parseSpline);
    }

    static Codec<std::shared_ptr<DensityFunction>> codec(
        const Registry<INoiseSampler>& noiseRegistry,
        const Registry<DensityFunction>& densityRegistry
    ) {
        return Codec<std::shared_ptr<DensityFunction>>([&noiseRegistry, &densityRegistry](
            simdjson::ondemand::value json
        ) -> DecodeResult<std::shared_ptr<DensityFunction>> {
            simdjson::ondemand::json_type type;
            auto type_error = json.type().get(type);
            if (type_error) {
                return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Failed to determine JSON type");
            }

            if (type == simdjson::ondemand::json_type::string) {
                std::string_view ref;
                auto error = json.get_string().get(ref);
                if (error) {
                    return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Expected string");
                }
                std::string id(ref);
                auto func = densityRegistry.get(id);
                if (!func) {
                    return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Unknown density function: " + id);
                }
                return DecodeResult<std::shared_ptr<DensityFunction>>::success(func);
            }

            if (type == simdjson::ondemand::json_type::number) {
                auto result = Codecs::DOUBLE().decode(json);
                if (result.isError()) {
                    return DecodeResult<std::shared_ptr<DensityFunction>>::failure(result.error);
                }
                return DecodeResult<std::shared_ptr<DensityFunction>>::success(
                    std::make_shared<ConstantFunction>(result.value.value())
                );
            }

            simdjson::ondemand::object obj;
            auto error = json.get_object().get(obj);
            if (error) {
                return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Expected object");
            }

            simdjson::ondemand::value typeVal;
            error = obj["type"].get(typeVal);
            if (error) {
                return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Missing 'type' field");
            }

            std::string_view typeView;
            error = typeVal.get_string().get(typeView);
            if (error) {
                return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Type must be string");
            }

            std::string typeName(typeView);
            auto it = s_parsers.find(typeName);
            if (it == s_parsers.end()) {
                return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Unknown type: " + typeName);
            }

            return it->second(json, noiseRegistry, densityRegistry);
        });
    }

private:
    static inline std::unordered_map<std::string, Parser> s_parsers;

    static void registerType(const std::string& name, Parser parser) {
        s_parsers[name] = std::move(parser);
    }

    static DecodeResult<std::shared_ptr<DensityFunction>> parseConstant(
        simdjson::ondemand::value json,
        const Registry<INoiseSampler>&,
        const Registry<DensityFunction>&
    ) {
        simdjson::ondemand::object obj;
        auto error = json.get_object().get(obj);
        if (error) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Expected object");
        }

        auto valueField = field("argument", Codecs::DOUBLE());
        auto result = valueField.decode(obj);
        if (result.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure(result.error);
        }

        return DecodeResult<std::shared_ptr<DensityFunction>>::success(
            std::make_shared<ConstantFunction>(result.value.value())
        );
    }

    static DecodeResult<std::shared_ptr<DensityFunction>> parseYClampedGradient(
        simdjson::ondemand::value json,
        const Registry<INoiseSampler>&,
        const Registry<DensityFunction>&
    ) {
        auto result = YClampedGradientFunction::codec().decode(json);
        if (result.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure(result.error);
        }
        return DecodeResult<std::shared_ptr<DensityFunction>>::success(
            std::make_shared<YClampedGradientFunction>(std::move(result.value.value()))
        );
    }

    static DecodeResult<std::shared_ptr<DensityFunction>> parseNoise(
        simdjson::ondemand::value json,
        const Registry<INoiseSampler>& noiseRegistry,
        const Registry<DensityFunction>&
    ) {
        simdjson::ondemand::object obj;
        auto error = json.get_object().get(obj);
        if (error) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Expected object");
        }

        auto noiseField = field("noise", Codecs::STRING());
        auto xzScaleField = optionalField("xzScale", Codecs::DOUBLE(), 1.0);
        auto yScaleField = optionalField("yScale", Codecs::DOUBLE(), 1.0);

        auto noiseResult = noiseField.decode(obj);
        if (noiseResult.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure(noiseResult.error);
        }

        auto noise = noiseRegistry.get(noiseResult.value.value());
        if (!noise) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure(
                "Unknown noise: " + noiseResult.value.value()
            );
        }

        auto xzScale = xzScaleField.decode(obj);
        if (xzScale.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure(xzScale.error);
        }

        auto yScale = yScaleField.decode(obj);
        if (yScale.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure(yScale.error);
        }

        return DecodeResult<std::shared_ptr<DensityFunction>>::success(
            std::make_shared<NoiseFunction>(noise, xzScale.value.value(), yScale.value.value())
        );
    }

    static DecodeResult<std::shared_ptr<DensityFunction>> parseInterpolated(
        simdjson::ondemand::value json,
        const Registry<INoiseSampler>&,
        const Registry<DensityFunction>&
    ) {
        simdjson::ondemand::object obj;
        auto error = json.get_object().get(obj);
        if (error) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Expected object");
        }

        auto xzScaleField = field("xzScale", Codecs::DOUBLE());
        auto yScaleField = field("yScale", Codecs::DOUBLE());
        auto xzFactorField = field("xzFactor", Codecs::DOUBLE());
        auto yFactorField = field("yFactor", Codecs::DOUBLE());
        auto smearField = field("smearScaleMultiplier", Codecs::DOUBLE());

        auto xzScale = xzScaleField.decode(obj);
        if (xzScale.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure(xzScale.error);
        }

        obj.reset();
        auto yScale = yScaleField.decode(obj);
        if (yScale.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure(yScale.error);
        }

        obj.reset();
        auto xzFactor = xzFactorField.decode(obj);
        if (xzFactor.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure(xzFactor.error);
        }

        obj.reset();
        auto yFactor = yFactorField.decode(obj);
        if (yFactor.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure(yFactor.error);
        }

        obj.reset();
        auto smear = smearField.decode(obj);
        if (smear.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure(smear.error);
        }

        // Create InterpolatedNoiseSampler and wrap it in a NoiseFunction
        // We use seed=0 since it gets overridden by the world seed anyway
        auto sampler = std::make_shared<InterpolatedNoiseSampler>(
            0,
            xzScale.value.value(),
            yScale.value.value(),
            xzFactor.value.value(),
            yFactor.value.value(),
            smear.value.value()
        );

        // Wrap in NoiseFunction with scale 1.0 since InterpolatedNoiseSampler does its own scaling
        return DecodeResult<std::shared_ptr<DensityFunction>>::success(
            std::make_shared<NoiseFunction>(sampler, 1.0, 1.0)
        );
    }

    static DecodeResult<std::shared_ptr<DensityFunction>> parseAdd(
        simdjson::ondemand::value json,
        const Registry<INoiseSampler>& noiseRegistry,
        const Registry<DensityFunction>& densityRegistry
    ) {
        return parseBinaryOp<AddFunction>(json, noiseRegistry, densityRegistry);
    }

    static DecodeResult<std::shared_ptr<DensityFunction>> parseMul(
        simdjson::ondemand::value json,
        const Registry<INoiseSampler>& noiseRegistry,
        const Registry<DensityFunction>& densityRegistry
    ) {
        return parseBinaryOp<MulFunction>(json, noiseRegistry, densityRegistry);
    }

    static DecodeResult<std::shared_ptr<DensityFunction>> parseMin(
        simdjson::ondemand::value json,
        const Registry<INoiseSampler>& noiseRegistry,
        const Registry<DensityFunction>& densityRegistry
    ) {
        return parseBinaryOp<MinFunction>(json, noiseRegistry, densityRegistry);
    }

    static DecodeResult<std::shared_ptr<DensityFunction>> parseMax(
        simdjson::ondemand::value json,
        const Registry<INoiseSampler>& noiseRegistry,
        const Registry<DensityFunction>& densityRegistry
    ) {
        return parseBinaryOp<MaxFunction>(json, noiseRegistry, densityRegistry);
    }

    template<typename T>
    static DecodeResult<std::shared_ptr<DensityFunction>> parseBinaryOp(
        simdjson::ondemand::value json,
        const Registry<INoiseSampler>& noiseRegistry,
        const Registry<DensityFunction>& densityRegistry
    ) {
        simdjson::ondemand::object obj;
        auto error = json.get_object().get(obj);
        if (error) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Expected object");
        }

        auto dfCodec = codec(noiseRegistry, densityRegistry);

        simdjson::ondemand::value arg1Val;
        error = obj["argument1"].get(arg1Val);
        if (error) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Missing 'argument1'");
        }

        auto arg1 = dfCodec.decode(arg1Val);
        if (arg1.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("argument1: " + arg1.error);
        }

        simdjson::ondemand::value arg2Val;
        error = obj["argument2"].get(arg2Val);
        if (error) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Missing 'argument2'");
        }

        auto arg2 = dfCodec.decode(arg2Val);
        if (arg2.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("argument2: " + arg2.error);
        }

        return DecodeResult<std::shared_ptr<DensityFunction>>::success(
            std::make_shared<T>(arg1.value.value(), arg2.value.value())
        );
    }

    static DecodeResult<std::shared_ptr<DensityFunction>> parseClamp(
        simdjson::ondemand::value json,
        const Registry<INoiseSampler>& noiseRegistry,
        const Registry<DensityFunction>& densityRegistry
    ) {
        simdjson::ondemand::object obj;
        auto error = json.get_object().get(obj);
        if (error) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Expected object");
        }

        auto inputField = field("input", codec(noiseRegistry, densityRegistry));
        auto minField = field("min", Codecs::DOUBLE());
        auto maxField = field("max", Codecs::DOUBLE());

        auto input = inputField.decode(obj);
        if (input.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure(input.error);
        }

        auto min = minField.decode(obj);
        if (min.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure(min.error);
        }

        auto max = maxField.decode(obj);
        if (max.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure(max.error);
        }

        return DecodeResult<std::shared_ptr<DensityFunction>>::success(
            std::make_shared<ClampFunction>(
                input.value.value(),
                min.value.value(),
                max.value.value()
            )
        );
    }

    static DecodeResult<std::shared_ptr<DensityFunction>> parseAbs(
        simdjson::ondemand::value json,
        const Registry<INoiseSampler>& noiseRegistry,
        const Registry<DensityFunction>& densityRegistry
    ) {
        return parseUnaryOp<AbsFunction>(json, noiseRegistry, densityRegistry);
    }

    static DecodeResult<std::shared_ptr<DensityFunction>> parseSquare(
        simdjson::ondemand::value json,
        const Registry<INoiseSampler>& noiseRegistry,
        const Registry<DensityFunction>& densityRegistry
    ) {
        return parseUnaryOp<SquareFunction>(json, noiseRegistry, densityRegistry);
    }

    static DecodeResult<std::shared_ptr<DensityFunction>> parseQuarterNegative(
        simdjson::ondemand::value json,
        const Registry<INoiseSampler>& noiseRegistry,
        const Registry<DensityFunction>& densityRegistry
    ) {
        return parseUnaryOp<QuarterNegativeFunction>(json, noiseRegistry, densityRegistry);
    }

    static DecodeResult<std::shared_ptr<DensityFunction>> parseLerp(
        simdjson::ondemand::value json,
        const Registry<INoiseSampler>& noiseRegistry,
        const Registry<DensityFunction>& densityRegistry
    ) {
        simdjson::ondemand::object obj;
        auto error = json.get_object().get(obj);
        if (error) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Expected object");
        }

        auto dfCodec = codec(noiseRegistry, densityRegistry);

        simdjson::ondemand::value tVal;
        error = obj["t"].get(tVal);
        if (error) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Missing 't'");
        }

        auto t = dfCodec.decode(tVal);
        if (t.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("t: " + t.error);
        }

        simdjson::ondemand::value aVal;
        error = obj["a"].get(aVal);
        if (error) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Missing 'a'");
        }

        auto a = dfCodec.decode(aVal);
        if (a.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("a: " + a.error);
        }

        simdjson::ondemand::value bVal;
        error = obj["b"].get(bVal);
        if (error) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Missing 'b'");
        }

        auto b = dfCodec.decode(bVal);
        if (b.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("b: " + b.error);
        }

        return DecodeResult<std::shared_ptr<DensityFunction>>::success(
            std::make_shared<LerpFunction>(t.value.value(), a.value.value(), b.value.value())
        );
    }

    template<typename T>
    static DecodeResult<std::shared_ptr<DensityFunction>> parseUnaryOp(
        simdjson::ondemand::value json,
        const Registry<INoiseSampler>& noiseRegistry,
        const Registry<DensityFunction>& densityRegistry
    ) {
        simdjson::ondemand::object obj;
        auto error = json.get_object().get(obj);
        if (error) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Expected object");
        }

        auto inputField = field("argument", codec(noiseRegistry, densityRegistry));
        auto result = inputField.decode(obj);
        if (result.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure(result.error);
        }

        return DecodeResult<std::shared_ptr<DensityFunction>>::success(
            std::make_shared<T>(result.value.value())
        );
    }

    static DecodeResult<std::shared_ptr<DensityFunction>> parseSpline(
        simdjson::ondemand::value json,
        const Registry<INoiseSampler>& noiseRegistry,
        const Registry<DensityFunction>& densityRegistry
    ) {
        simdjson::ondemand::object obj;
        auto error = json.get_object().get(obj);
        if (error) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Expected object");
        }

        auto inputField = field("coordinate", codec(noiseRegistry, densityRegistry));
        auto input = inputField.decode(obj);
        if (input.isError()) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure(input.error);
        }

        simdjson::ondemand::value pointsVal;
        error = obj["points"].get(pointsVal);
        if (error) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Missing 'points'");
        }

        simdjson::ondemand::array pointsArr;
        error = pointsVal.get_array().get(pointsArr);
        if (error) {
            return DecodeResult<std::shared_ptr<DensityFunction>>::failure("Points must be array");
        }

        std::vector<SplinePoint> points;
        for (auto pointVal : pointsArr) {
            simdjson::ondemand::object pointObj;
            error = pointVal.get_object().get(pointObj);
            if (error) continue;

            auto locField = field("location", Codecs::FLOAT());
            auto valField = field("value", Codecs::FLOAT());
            auto derivField = optionalField("derivative", Codecs::FLOAT(), 0.0f);

            auto loc = locField.decode(pointObj);
            if (loc.isError()) continue;

            auto val = valField.decode(pointObj);
            if (val.isError()) continue;

            auto deriv = derivField.decode(pointObj);

            points.push_back({
                loc.value.value(),
                val.value.value(),
                deriv.value.value()
            });
        }

        return DecodeResult<std::shared_ptr<DensityFunction>>::success(
            std::make_shared<SplineFunction>(input.value.value(), std::move(points))
        );
    }
};

} // namespace VoxelEngine