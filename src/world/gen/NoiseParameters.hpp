#pragma once
#include "Codec.hpp"
#include <FastNoise/FastNoise.h>
#include <memory>
#include <string>

namespace VoxelEngine {

struct NoiseParameters {
    int firstOctave;
    std::vector<double> amplitudes;

    NoiseParameters(int firstOctave, std::vector<double> amplitudes)
        : firstOctave(firstOctave), amplitudes(std::move(amplitudes)) {}

    int getOctaveCount() const {
        return static_cast<int>(amplitudes.size());
    }

    double getAmplitude(int octave) const {
        int index = octave - firstOctave;
        if (index < 0 || index >= static_cast<int>(amplitudes.size())) {
            return 0.0;
        }
        return amplitudes[index];
    }

    double getMaxValue() const {
        double max = 0.0;
        for (double amp : amplitudes) {
            max += std::abs(amp);
        }
        return max;
    }

    static Codec<NoiseParameters> codec() {
        return Codec<NoiseParameters>([](simdjson::ondemand::value json) -> DecodeResult<NoiseParameters> {
            simdjson::ondemand::object obj;
            auto error = json.get_object().get(obj);
            if (error) {
                return DecodeResult<NoiseParameters>::failure("Expected object");
            }

            auto firstOctaveField = field("firstOctave", Codecs::INT32());
            auto amplitudesField = field("amplitudes", Codecs::list(Codecs::DOUBLE()));

            auto firstOctaveResult = firstOctaveField.decode(obj);
            if (firstOctaveResult.isError()) {
                return DecodeResult<NoiseParameters>::failure(firstOctaveResult.error);
            }

            obj.reset();
            auto amplitudesResult = amplitudesField.decode(obj);
            if (amplitudesResult.isError()) {
                return DecodeResult<NoiseParameters>::failure(amplitudesResult.error);
            }

            return DecodeResult<NoiseParameters>::success(
                NoiseParameters(
                    firstOctaveResult.value.value(),
                    std::move(amplitudesResult.value.value())
                )
            );
        });
    }
};

enum class NoiseType {
    PERLIN,
    SIMPLEX,
    CELLULAR,
    VALUE
};

// Base interface for all noise samplers
class INoiseSampler {
public:
    virtual ~INoiseSampler() = default;
    virtual double sample(double x, double y, double z) const = 0;
    virtual double sample2D(double x, double z) const = 0;
    virtual void sampleGrid(float* output, int xSize, int ySize, int zSize,
                           double xStart, double yStart, double zStart,
                           double xStep, double yStep, double zStep) const = 0;
};

class NoiseGenerator : public INoiseSampler {
public:
    NoiseGenerator(NoiseType type, const NoiseParameters& params, int seed)
        : m_params(params), m_seed(seed) {
        createGenerator(type);
    }

    double sample(double x, double y, double z) const override {
        if (!m_generator) {
            return 0.0;
        }

        double result = 0.0;

        // Calculate initial frequency from firstOctave using pow instead of bit shift
        // firstOctave is typically negative (e.g., -9), so 2^(-9) = 0.001953125
        double frequency = std::pow(2.0, m_params.firstOctave);
        double amplitude = 1.0;

        for (int octave = 0; octave < m_params.getOctaveCount(); octave++) {
            int actualOctave = m_params.firstOctave + octave;
            double octaveAmplitude = m_params.getAmplitude(actualOctave);

            if (octaveAmplitude != 0.0) {
                float sample = m_generator->GenSingle3D(
                    static_cast<float>(x * frequency),
                    static_cast<float>(y * frequency),
                    static_cast<float>(z * frequency),
                    m_seed + actualOctave
                );
                result += sample * octaveAmplitude * amplitude;
            }

            // Each octave doubles frequency and halves amplitude
            frequency *= 2.0;
            amplitude *= 0.5;
        }

        return result;
    }

    double sample2D(double x, double z) const override {
        return sample(x, 0.0, z);
    }

    // Batch sampling using SIMD - fills output array with noise values
    void sampleGrid(float* output, int xSize, int ySize, int zSize,
                   double xStart, double yStart, double zStart,
                   double xStep, double yStep, double zStep) const override {
        if (!m_generator) {
            std::fill(output, output + (xSize * ySize * zSize), 0.0f);
            return;
        }

        const int totalSize = xSize * ySize * zSize;
        std::fill(output, output + totalSize, 0.0f);

        std::vector<float> tempBuffer(totalSize);

        // Calculate initial frequency from firstOctave using pow instead of bit shift
        double frequency = std::pow(2.0, m_params.firstOctave);
        double amplitude = 1.0;

        for (int octave = 0; octave < m_params.getOctaveCount(); octave++) {
            int actualOctave = m_params.firstOctave + octave;
            double octaveAmplitude = m_params.getAmplitude(actualOctave);

            if (octaveAmplitude != 0.0) {
                // GenUniformGrid3D takes integer start positions and a frequency
                // It generates from (xStart, yStart, zStart) with the given frequency
                m_generator->GenUniformGrid3D(
                    tempBuffer.data(),
                    static_cast<int>(xStart * frequency),
                    static_cast<int>(yStart * frequency),
                    static_cast<int>(zStart * frequency),
                    xSize, ySize, zSize,
                    static_cast<float>(xStep * frequency),
                    m_seed + actualOctave
                );

                // Accumulate this octave with amplitude
                float combinedAmplitude = static_cast<float>(octaveAmplitude * amplitude);
                for (int i = 0; i < totalSize; i++) {
                    output[i] += tempBuffer[i] * combinedAmplitude;
                }
            }

            // Each octave doubles frequency and halves amplitude
            frequency *= 2.0;
            amplitude *= 0.5;
        }
    }

private:
    void createGenerator(NoiseType type) {
        switch (type) {
            case NoiseType::PERLIN:
                m_generator = FastNoise::New<FastNoise::Perlin>();
                break;
            case NoiseType::SIMPLEX:
                m_generator = FastNoise::New<FastNoise::Simplex>();
                break;
            case NoiseType::CELLULAR:
                m_generator = FastNoise::New<FastNoise::CellularValue>();
                break;
            case NoiseType::VALUE:
                m_generator = FastNoise::New<FastNoise::Value>();
                break;
        }
    }

    NoiseParameters m_params;
    int m_seed;
    FastNoise::SmartNode<> m_generator;
};

// Minecraft-style DoublePerlinNoise that uses two octave samplers with domain rotation
class DoublePerlinNoiseGenerator : public INoiseSampler {
public:
    DoublePerlinNoiseGenerator(NoiseType type, const NoiseParameters& params, int seed)
        : m_firstSampler(type, params, seed),
          m_secondSampler(type, params, seed + 1),
          m_amplitude(calculateAmplitude(params)) {}

    double sample(double x, double y, double z) const override {
        // Domain rotation constant from Minecraft
        constexpr double DOMAIN_ROTATION = 1.0181268882175227;

        // Sample first noise normally
        double first = m_firstSampler.sample(x, y, z);

        // Sample second noise with rotated coordinates
        double second = m_secondSampler.sample(
            x * DOMAIN_ROTATION,
            y * DOMAIN_ROTATION,
            z * DOMAIN_ROTATION
        );

        // Combine and apply amplitude
        return (first + second) * m_amplitude;
    }

    double sample2D(double x, double z) const override {
        return sample(x, 0.0, z);
    }

    void sampleGrid(float* output, int xSize, int ySize, int zSize,
                   double xStart, double yStart, double zStart,
                   double xStep, double yStep, double zStep) const override {
        constexpr double DOMAIN_ROTATION = 1.0181268882175227;

        const int totalSize = xSize * ySize * zSize;
        std::vector<float> firstBuffer(totalSize);
        std::vector<float> secondBuffer(totalSize);

        // Sample first noise
        m_firstSampler.sampleGrid(
            firstBuffer.data(),
            xSize, ySize, zSize,
            xStart, yStart, zStart,
            xStep, yStep, zStep
        );

        // Sample second noise with rotated coordinates
        m_secondSampler.sampleGrid(
            secondBuffer.data(),
            xSize, ySize, zSize,
            xStart * DOMAIN_ROTATION,
            yStart * DOMAIN_ROTATION,
            zStart * DOMAIN_ROTATION,
            xStep * DOMAIN_ROTATION,
            yStep * DOMAIN_ROTATION,
            zStep * DOMAIN_ROTATION
        );

        // Combine both samples with amplitude
        float amp = static_cast<float>(m_amplitude);
        for (int i = 0; i < totalSize; i++) {
            output[i] = (firstBuffer[i] + secondBuffer[i]) * amp;
        }
    }

private:
    double calculateAmplitude(const NoiseParameters& params) const {
        // Minecraft's amplitude calculation
        // amplitude = 0.16666666666666666 / createAmplitude(octaveRange)
        // where createAmplitude(n) = 0.1 * (1.0 + 1.0/(n+1))
        int octaveCount = params.getOctaveCount();
        double createAmplitude = 0.1 * (1.0 + 1.0 / (octaveCount + 1));
        return 0.16666666666666666 / createAmplitude;
    }

    NoiseGenerator m_firstSampler;
    NoiseGenerator m_secondSampler;
    double m_amplitude;
};

} // namespace VoxelEngine