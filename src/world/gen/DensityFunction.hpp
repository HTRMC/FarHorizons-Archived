#pragma once
#include "Codec.hpp"
#include "Registry.hpp"
#include "NoiseParameters.hpp"
#include <memory>
#include <cmath>
#include <algorithm>

namespace VoxelEngine {

struct DensityContext {
    double x, y, z;
    int blockX, blockY, blockZ;
};

class DensityFunction {
public:
    virtual ~DensityFunction() = default;
    virtual double compute(const DensityContext& ctx) const = 0;
    virtual double minValue() const = 0;
    virtual double maxValue() const = 0;

    // Default batch implementation - can be overridden for performance
    virtual void computeGrid(float* output, int xSize, int ySize, int zSize,
                            double xStart, double yStart, double zStart,
                            double xStep, double yStep, double zStep,
                            int horizontalSize, int verticalSize) const {
        // Fallback: compute each point individually
        int index = 0;
        for (int z = 0; z < zSize; z++) {
            for (int y = 0; y < ySize; y++) {
                for (int x = 0; x < xSize; x++) {
                    DensityContext ctx;
                    ctx.x = (xStart + x * xStep) / horizontalSize;
                    ctx.y = (yStart + y * yStep) / verticalSize;
                    ctx.z = (zStart + z * zStep) / horizontalSize;
                    ctx.blockX = static_cast<int>(xStart + x * xStep);
                    ctx.blockY = static_cast<int>(yStart + y * yStep);
                    ctx.blockZ = static_cast<int>(zStart + z * zStep);
                    output[index++] = static_cast<float>(compute(ctx));
                }
            }
        }
    }
};

class ConstantFunction : public DensityFunction {
public:
    explicit ConstantFunction(double value) : m_value(value) {}

    double compute(const DensityContext&) const override {
        return m_value;
    }

    double minValue() const override { return m_value; }
    double maxValue() const override { return m_value; }

    static Codec<ConstantFunction> codec() {
        return Codec<ConstantFunction>([](simdjson::ondemand::value json) -> DecodeResult<ConstantFunction> {
            auto result = Codecs::DOUBLE().decode(json);
            if (result.isError()) {
                return DecodeResult<ConstantFunction>::failure(result.error);
            }
            return DecodeResult<ConstantFunction>::success(ConstantFunction(result.value.value()));
        });
    }

private:
    double m_value;
};

class YClampedGradientFunction : public DensityFunction {
public:
    YClampedGradientFunction(int fromY, int toY, double fromValue, double toValue)
        : m_fromY(fromY), m_toY(toY), m_fromValue(fromValue), m_toValue(toValue) {}

    double compute(const DensityContext& ctx) const override {
        if (ctx.blockY <= m_fromY) return m_fromValue;
        if (ctx.blockY >= m_toY) return m_toValue;

        double t = static_cast<double>(ctx.blockY - m_fromY) / (m_toY - m_fromY);
        return m_fromValue + t * (m_toValue - m_fromValue);
    }

    double minValue() const override { return std::min(m_fromValue, m_toValue); }
    double maxValue() const override { return std::max(m_fromValue, m_toValue); }

    static Codec<YClampedGradientFunction> codec() {
        return Codec<YClampedGradientFunction>([](simdjson::ondemand::value json) -> DecodeResult<YClampedGradientFunction> {
            simdjson::ondemand::object obj;
            auto error = json.get_object().get(obj);
            if (error) {
                return DecodeResult<YClampedGradientFunction>::failure("Expected object");
            }

            auto fromYField = field("fromY", Codecs::INT32());
            auto toYField = field("toY", Codecs::INT32());
            auto fromValueField = field("fromValue", Codecs::DOUBLE());
            auto toValueField = field("toValue", Codecs::DOUBLE());

            auto fromY = fromYField.decode(obj);
            if (fromY.isError()) return DecodeResult<YClampedGradientFunction>::failure(fromY.error);

            obj.reset();
            auto toY = toYField.decode(obj);
            if (toY.isError()) return DecodeResult<YClampedGradientFunction>::failure(toY.error);

            obj.reset();
            auto fromValue = fromValueField.decode(obj);
            if (fromValue.isError()) return DecodeResult<YClampedGradientFunction>::failure(fromValue.error);

            obj.reset();
            auto toValue = toValueField.decode(obj);
            if (toValue.isError()) return DecodeResult<YClampedGradientFunction>::failure(toValue.error);

            return DecodeResult<YClampedGradientFunction>::success(
                YClampedGradientFunction(
                    fromY.value.value(),
                    toY.value.value(),
                    fromValue.value.value(),
                    toValue.value.value()
                )
            );
        });
    }

private:
    int m_fromY, m_toY;
    double m_fromValue, m_toValue;
};

class NoiseFunction : public DensityFunction {
public:
    NoiseFunction(std::shared_ptr<INoiseSampler> noise, double xzScale, double yScale)
        : m_noise(std::move(noise)), m_xzScale(xzScale), m_yScale(yScale) {}

    double compute(const DensityContext& ctx) const override {
        if (!m_noise) return 0.0;
        return m_noise->sample(ctx.x * m_xzScale, ctx.y * m_yScale, ctx.z * m_xzScale);
    }

    double minValue() const override { return -1.0; }
    double maxValue() const override { return 1.0; }

    // Optimized batch compute using SIMD
    void computeGrid(float* output, int xSize, int ySize, int zSize,
                    double xStart, double yStart, double zStart,
                    double xStep, double yStep, double zStep,
                    int horizontalSize, int verticalSize) const override {
        if (!m_noise) {
            std::fill(output, output + (xSize * ySize * zSize), 0.0f);
            return;
        }

        // Convert from world coordinates to noise coordinates
        double noiseXStart = (xStart / horizontalSize) * m_xzScale;
        double noiseYStart = (yStart / verticalSize) * m_yScale;
        double noiseZStart = (zStart / horizontalSize) * m_xzScale;
        double noiseXStep = (xStep / horizontalSize) * m_xzScale;
        double noiseYStep = (yStep / verticalSize) * m_yScale;
        double noiseZStep = (zStep / horizontalSize) * m_xzScale;

        m_noise->sampleGrid(output, xSize, ySize, zSize,
                           noiseXStart, noiseYStart, noiseZStart,
                           noiseXStep, noiseYStep, noiseZStep);
    }

private:
    std::shared_ptr<INoiseSampler> m_noise;
    double m_xzScale;
    double m_yScale;
};

class AddFunction : public DensityFunction {
public:
    AddFunction(std::shared_ptr<DensityFunction> a, std::shared_ptr<DensityFunction> b)
        : m_a(std::move(a)), m_b(std::move(b)) {}

    double compute(const DensityContext& ctx) const override {
        return m_a->compute(ctx) + m_b->compute(ctx);
    }

    double minValue() const override { return m_a->minValue() + m_b->minValue(); }
    double maxValue() const override { return m_a->maxValue() + m_b->maxValue(); }

private:
    std::shared_ptr<DensityFunction> m_a;
    std::shared_ptr<DensityFunction> m_b;
};

class MulFunction : public DensityFunction {
public:
    MulFunction(std::shared_ptr<DensityFunction> a, std::shared_ptr<DensityFunction> b)
        : m_a(std::move(a)), m_b(std::move(b)) {}

    double compute(const DensityContext& ctx) const override {
        return m_a->compute(ctx) * m_b->compute(ctx);
    }

    double minValue() const override {
        double min1 = m_a->minValue();
        double max1 = m_a->maxValue();
        double min2 = m_b->minValue();
        double max2 = m_b->maxValue();
        return std::min({min1 * min2, min1 * max2, max1 * min2, max1 * max2});
    }

    double maxValue() const override {
        double min1 = m_a->minValue();
        double max1 = m_a->maxValue();
        double min2 = m_b->minValue();
        double max2 = m_b->maxValue();
        return std::max({min1 * min2, min1 * max2, max1 * min2, max1 * max2});
    }

private:
    std::shared_ptr<DensityFunction> m_a;
    std::shared_ptr<DensityFunction> m_b;
};

class MinFunction : public DensityFunction {
public:
    MinFunction(std::shared_ptr<DensityFunction> a, std::shared_ptr<DensityFunction> b)
        : m_a(std::move(a)), m_b(std::move(b)) {}

    double compute(const DensityContext& ctx) const override {
        return std::min(m_a->compute(ctx), m_b->compute(ctx));
    }

    double minValue() const override { return std::min(m_a->minValue(), m_b->minValue()); }
    double maxValue() const override { return std::min(m_a->maxValue(), m_b->maxValue()); }

private:
    std::shared_ptr<DensityFunction> m_a;
    std::shared_ptr<DensityFunction> m_b;
};

class MaxFunction : public DensityFunction {
public:
    MaxFunction(std::shared_ptr<DensityFunction> a, std::shared_ptr<DensityFunction> b)
        : m_a(std::move(a)), m_b(std::move(b)) {}

    double compute(const DensityContext& ctx) const override {
        return std::max(m_a->compute(ctx), m_b->compute(ctx));
    }

    double minValue() const override { return std::max(m_a->minValue(), m_b->minValue()); }
    double maxValue() const override { return std::max(m_a->maxValue(), m_b->maxValue()); }

private:
    std::shared_ptr<DensityFunction> m_a;
    std::shared_ptr<DensityFunction> m_b;
};

class ClampFunction : public DensityFunction {
public:
    ClampFunction(std::shared_ptr<DensityFunction> input, double min, double max)
        : m_input(std::move(input)), m_min(min), m_max(max) {}

    double compute(const DensityContext& ctx) const override {
        return std::clamp(m_input->compute(ctx), m_min, m_max);
    }

    double minValue() const override { return m_min; }
    double maxValue() const override { return m_max; }

private:
    std::shared_ptr<DensityFunction> m_input;
    double m_min, m_max;
};

class AbsFunction : public DensityFunction {
public:
    explicit AbsFunction(std::shared_ptr<DensityFunction> input)
        : m_input(std::move(input)) {}

    double compute(const DensityContext& ctx) const override {
        return std::abs(m_input->compute(ctx));
    }

    double minValue() const override {
        double min = m_input->minValue();
        double max = m_input->maxValue();
        if (min >= 0) return min;
        if (max <= 0) return -max;
        return 0.0;
    }

    double maxValue() const override {
        return std::max(std::abs(m_input->minValue()), std::abs(m_input->maxValue()));
    }

private:
    std::shared_ptr<DensityFunction> m_input;
};

class SquareFunction : public DensityFunction {
public:
    explicit SquareFunction(std::shared_ptr<DensityFunction> input)
        : m_input(std::move(input)) {}

    double compute(const DensityContext& ctx) const override {
        double val = m_input->compute(ctx);
        return val * val;
    }

    double minValue() const override {
        double min = m_input->minValue();
        double max = m_input->maxValue();
        if (min >= 0) return min * min;
        if (max <= 0) return max * max;
        return 0.0;
    }

    double maxValue() const override {
        double min = m_input->minValue();
        double max = m_input->maxValue();
        return std::max(min * min, max * max);
    }

private:
    std::shared_ptr<DensityFunction> m_input;
};

struct SplinePoint {
    float location;
    float value;
    float derivative;
};

class SplineFunction : public DensityFunction {
public:
    SplineFunction(std::shared_ptr<DensityFunction> input, std::vector<SplinePoint> points)
        : m_input(std::move(input)), m_points(std::move(points)) {
        std::sort(m_points.begin(), m_points.end(),
                  [](const SplinePoint& a, const SplinePoint& b) { return a.location < b.location; });
    }

    double compute(const DensityContext& ctx) const override {
        float t = static_cast<float>(m_input->compute(ctx));

        if (m_points.empty()) return 0.0;
        if (t <= m_points.front().location) return m_points.front().value;
        if (t >= m_points.back().location) return m_points.back().value;

        for (size_t i = 0; i < m_points.size() - 1; i++) {
            if (t >= m_points[i].location && t <= m_points[i + 1].location) {
                return cubicInterpolate(m_points[i], m_points[i + 1], t);
            }
        }

        return m_points.back().value;
    }

    double minValue() const override {
        if (m_points.empty()) return 0.0;
        double min = m_points[0].value;
        for (const auto& p : m_points) {
            min = std::min(min, static_cast<double>(p.value));
        }
        return min;
    }

    double maxValue() const override {
        if (m_points.empty()) return 0.0;
        double max = m_points[0].value;
        for (const auto& p : m_points) {
            max = std::max(max, static_cast<double>(p.value));
        }
        return max;
    }

private:
    double cubicInterpolate(const SplinePoint& p0, const SplinePoint& p1, float t) const {
        float dt = p1.location - p0.location;
        float u = (t - p0.location) / dt;

        float a = p0.value;
        float b = p0.derivative * dt;
        float c = 3 * (p1.value - p0.value) - 2 * p0.derivative * dt - p1.derivative * dt;
        float d = 2 * (p0.value - p1.value) + p0.derivative * dt + p1.derivative * dt;

        return a + b * u + c * u * u + d * u * u * u;
    }

    std::shared_ptr<DensityFunction> m_input;
    std::vector<SplinePoint> m_points;
};

} // namespace VoxelEngine