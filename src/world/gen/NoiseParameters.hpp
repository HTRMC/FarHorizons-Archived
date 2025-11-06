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

class NoiseGenerator {
public:
    NoiseGenerator(NoiseType type, const NoiseParameters& params, int seed)
        : m_params(params), m_seed(seed) {
        createGenerator(type);
    }

    double sample(double x, double y, double z) const {
        if (!m_generator) {
            return 0.0;
        }

        double result = 0.0;
        double currentFreq = 1.0;

        for (int octave = 0; octave < m_params.getOctaveCount(); octave++) {
            int actualOctave = m_params.firstOctave + octave;
            double amplitude = m_params.getAmplitude(actualOctave);

            if (amplitude != 0.0) {
                double freq = currentFreq * (1 << actualOctave);
                float sample = m_generator->GenSingle3D(
                    static_cast<float>(x * freq),
                    static_cast<float>(y * freq),
                    static_cast<float>(z * freq),
                    m_seed + actualOctave
                );
                result += sample * amplitude;
            }
        }

        return result;
    }

    double sample2D(double x, double z) const {
        return sample(x, 0.0, z);
    }

    // Batch sampling using SIMD - fills output array with noise values
    void sampleGrid(float* output, int xSize, int ySize, int zSize,
                   double xStart, double yStart, double zStart,
                   double xStep, double yStep, double zStep) const {
        if (!m_generator) {
            std::fill(output, output + (xSize * ySize * zSize), 0.0f);
            return;
        }

        const int totalSize = xSize * ySize * zSize;
        std::fill(output, output + totalSize, 0.0f);

        std::vector<float> tempBuffer(totalSize);

        for (int octave = 0; octave < m_params.getOctaveCount(); octave++) {
            int actualOctave = m_params.firstOctave + octave;
            double amplitude = m_params.getAmplitude(actualOctave);

            if (amplitude != 0.0) {
                float freq = static_cast<float>(1.0 * (1 << actualOctave));

                // GenUniformGrid3D takes integer start positions and a frequency
                // It generates from (xStart, yStart, zStart) with the given frequency
                m_generator->GenUniformGrid3D(
                    tempBuffer.data(),
                    static_cast<int>(xStart * freq),
                    static_cast<int>(yStart * freq),
                    static_cast<int>(zStart * freq),
                    xSize, ySize, zSize,
                    static_cast<float>(xStep * freq),
                    m_seed + actualOctave
                );

                // Accumulate this octave with amplitude
                for (int i = 0; i < totalSize; i++) {
                    output[i] += tempBuffer[i] * static_cast<float>(amplitude);
                }
            }
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

} // namespace VoxelEngine