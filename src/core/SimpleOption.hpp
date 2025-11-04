#pragma once

#include <string>
#include <functional>
#include <optional>
#include <memory>
#include <variant>
#include <vector>
#include <simdjson.h>
#include <spdlog/spdlog.h>
#include <magic_enum/magic_enum.hpp>

namespace VoxelEngine {

/**
 * Minecraft-style flexible option system with validation, serialization, and callbacks.
 * Supports any type T with proper codec implementation.
 */
template<typename T>
class SimpleOption {
public:
    using ValueTextGetter = std::function<std::string(const std::string&, const T&)>;
    using ChangeCallback = std::function<void(const T&)>;
    using Validator = std::function<std::optional<T>(const T&)>;

    /**
     * Codec for serializing/deserializing values to/from JSON
     */
    class Codec {
    public:
        virtual ~Codec() = default;

        // Encode value to JSON string
        virtual std::string encode(const T& value) const = 0;

        // Decode JSON to value, returns nullopt on error
        virtual std::optional<T> decode(const std::string& json) const = 0;

        // Decode from simdjson ondemand value
        virtual std::optional<T> decode(simdjson::ondemand::value& value) const = 0;

        // Decode from simdjson DOM element
        virtual std::optional<T> decode(simdjson::dom::element& element) const = 0;
    };

    /**
     * Create a simple option
     * @param key Translation key (e.g., "options.fov")
     * @param codec Serialization codec
     * @param defaultValue Default value
     * @param validator Optional validator function
     * @param changeCallback Called when value changes
     */
    SimpleOption(
        std::string key,
        std::shared_ptr<Codec> codec,
        T defaultValue,
        Validator validator = nullptr,
        ChangeCallback changeCallback = nullptr
    ) : m_key(std::move(key)),
        m_codec(std::move(codec)),
        m_defaultValue(defaultValue),
        m_value(defaultValue),
        m_validator(std::move(validator)),
        m_changeCallback(std::move(changeCallback)) {}

    // Get current value
    const T& getValue() const { return m_value; }

    // Implicit conversion to T for convenience
    operator const T&() const { return m_value; }

    // Comparison operators
    bool operator==(const T& other) const { return m_value == other; }
    bool operator!=(const T& other) const { return m_value != other; }
    bool operator<(const T& other) const { return m_value < other; }
    bool operator>(const T& other) const { return m_value > other; }
    bool operator<=(const T& other) const { return m_value <= other; }
    bool operator>=(const T& other) const { return m_value >= other; }

    // Assignment from value
    SimpleOption& operator=(const T& value) {
        setValue(value);
        return *this;
    }

    // Set value (validates and triggers callback)
    void setValue(const T& value) {
        T validated = value;

        // Validate if validator exists
        if (m_validator) {
            auto result = m_validator(value);
            if (!result.has_value()) {
                spdlog::error("Invalid option value for {}, using default", m_key);
                validated = m_defaultValue;
            } else {
                validated = *result;
            }
        }

        // Only update if value changed
        if (m_value != validated) {
            m_value = validated;
            if (m_changeCallback) {
                m_changeCallback(m_value);
            }
        }
    }

    // Reset to default
    void reset() { setValue(m_defaultValue); }

    // Get key
    const std::string& getKey() const { return m_key; }

    // Serialize to JSON string
    std::string serialize() const {
        return m_codec->encode(m_value);
    }

    // Deserialize from JSON string
    bool deserialize(const std::string& json) {
        auto result = m_codec->decode(json);
        if (result.has_value()) {
            setValue(*result);
            return true;
        }
        return false;
    }

    // Deserialize from simdjson ondemand value
    bool deserialize(simdjson::ondemand::value& value) {
        auto result = m_codec->decode(value);
        if (result.has_value()) {
            setValue(*result);
            return true;
        }
        return false;
    }

    // Deserialize from simdjson DOM element
    bool deserialize(simdjson::dom::element& element) {
        auto result = m_codec->decode(element);
        if (result.has_value()) {
            setValue(*result);
            return true;
        }
        return false;
    }

private:
    std::string m_key;
    std::shared_ptr<Codec> m_codec;
    T m_defaultValue;
    T m_value;
    Validator m_validator;
    ChangeCallback m_changeCallback;
};

// ============================================================================
// Built-in Codecs
// ============================================================================

/**
 * Codec for integers
 */
class IntCodec : public SimpleOption<int32_t>::Codec {
public:
    IntCodec(int32_t minValue = INT32_MIN, int32_t maxValue = INT32_MAX)
        : m_min(minValue), m_max(maxValue) {}

    std::string encode(const int32_t& value) const override {
        return std::to_string(value);
    }

    std::optional<int32_t> decode(const std::string& json) const override {
        try {
            int32_t val = std::stoi(json);
            if (val >= m_min && val <= m_max) {
                return val;
            }
        } catch (...) {}
        return std::nullopt;
    }

    std::optional<int32_t> decode(simdjson::ondemand::value& value) const override {
        try {
            int64_t val = value.get_int64();
            if (val >= m_min && val <= m_max) {
                return static_cast<int32_t>(val);
            }
        } catch (...) {}
        return std::nullopt;
    }

    std::optional<int32_t> decode(simdjson::dom::element& element) const override {
        try {
            int64_t val = element.get_int64();
            if (val >= m_min && val <= m_max) {
                return static_cast<int32_t>(val);
            }
        } catch (...) {}
        return std::nullopt;
    }

private:
    int32_t m_min, m_max;
};

/**
 * Codec for floats/doubles
 */
class FloatCodec : public SimpleOption<float>::Codec {
public:
    FloatCodec(float minValue = -FLT_MAX, float maxValue = FLT_MAX)
        : m_min(minValue), m_max(maxValue) {}

    std::string encode(const float& value) const override {
        return std::to_string(value);
    }

    std::optional<float> decode(const std::string& json) const override {
        try {
            float val = std::stof(json);
            if (val >= m_min && val <= m_max) {
                return val;
            }
        } catch (...) {}
        return std::nullopt;
    }

    std::optional<float> decode(simdjson::ondemand::value& value) const override {
        try {
            double val = value.get_double();
            if (val >= m_min && val <= m_max) {
                return static_cast<float>(val);
            }
        } catch (...) {}
        return std::nullopt;
    }

    std::optional<float> decode(simdjson::dom::element& element) const override {
        try {
            double val = element.get_double();
            if (val >= m_min && val <= m_max) {
                return static_cast<float>(val);
            }
        } catch (...) {}
        return std::nullopt;
    }

private:
    float m_min, m_max;
};

/**
 * Codec for booleans
 */
class BoolCodec : public SimpleOption<bool>::Codec {
public:
    std::string encode(const bool& value) const override {
        return value ? "true" : "false";
    }

    std::optional<bool> decode(const std::string& json) const override {
        if (json == "true") return true;
        if (json == "false") return false;
        return std::nullopt;
    }

    std::optional<bool> decode(simdjson::ondemand::value& value) const override {
        try {
            return value.get_bool();
        } catch (...) {}
        return std::nullopt;
    }

    std::optional<bool> decode(simdjson::dom::element& element) const override {
        try {
            return element.get_bool();
        } catch (...) {}
        return std::nullopt;
    }
};

/**
 * Codec for strings
 */
class StringCodec : public SimpleOption<std::string>::Codec {
public:
    std::string encode(const std::string& value) const override {
        return "\"" + value + "\"";
    }

    std::optional<std::string> decode(const std::string& json) const override {
        // Remove quotes if present
        if (json.size() >= 2 && json.front() == '"' && json.back() == '"') {
            return json.substr(1, json.size() - 2);
        }
        return json;
    }

    std::optional<std::string> decode(simdjson::ondemand::value& value) const override {
        try {
            return std::string(value.get_string().value());
        } catch (...) {}
        return std::nullopt;
    }

    std::optional<std::string> decode(simdjson::dom::element& element) const override {
        try {
            std::string_view sv = element.get_string();
            return std::string(sv);
        } catch (...) {}
        return std::nullopt;
    }
};

/**
 * Codec for enums (using magic_enum)
 */
template<typename E>
class EnumCodec : public SimpleOption<E>::Codec {
public:
    std::string encode(const E& value) const override {
        auto name = magic_enum::enum_name(value);
        return "\"" + std::string(name) + "\"";
    }

    std::optional<E> decode(const std::string& json) const override {
        // Remove quotes if present
        std::string str = json;
        if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
            str = str.substr(1, str.size() - 2);
        }
        return magic_enum::enum_cast<E>(str);
    }

    std::optional<E> decode(simdjson::ondemand::value& value) const override {
        try {
            std::string str(value.get_string().value());
            return magic_enum::enum_cast<E>(str);
        } catch (...) {}
        return std::nullopt;
    }

    std::optional<E> decode(simdjson::dom::element& element) const override {
        try {
            std::string_view sv = element.get_string();
            return magic_enum::enum_cast<E>(std::string(sv));
        } catch (...) {}
        return std::nullopt;
    }
};

// ============================================================================
// Factory functions for common option types
// ============================================================================

/**
 * Create a boolean option
 */
inline SimpleOption<bool> ofBoolean(
    std::string key,
    bool defaultValue,
    SimpleOption<bool>::ChangeCallback callback = nullptr
) {
    return SimpleOption<bool>(
        std::move(key),
        std::make_shared<BoolCodec>(),
        defaultValue,
        nullptr,
        std::move(callback)
    );
}

/**
 * Create an integer option with range validation
 */
inline SimpleOption<int32_t> ofInt(
    std::string key,
    int32_t defaultValue,
    int32_t minValue,
    int32_t maxValue,
    SimpleOption<int32_t>::ChangeCallback callback = nullptr
) {
    auto validator = [minValue, maxValue](int32_t val) -> std::optional<int32_t> {
        if (val >= minValue && val <= maxValue) return val;
        return std::nullopt;
    };

    return SimpleOption<int32_t>(
        std::move(key),
        std::make_shared<IntCodec>(minValue, maxValue),
        defaultValue,
        validator,
        std::move(callback)
    );
}

/**
 * Create a float option with range validation
 */
inline SimpleOption<float> ofFloat(
    std::string key,
    float defaultValue,
    float minValue,
    float maxValue,
    SimpleOption<float>::ChangeCallback callback = nullptr
) {
    auto validator = [minValue, maxValue](float val) -> std::optional<float> {
        if (val >= minValue && val <= maxValue) return val;
        return std::nullopt;
    };

    return SimpleOption<float>(
        std::move(key),
        std::make_shared<FloatCodec>(minValue, maxValue),
        defaultValue,
        validator,
        std::move(callback)
    );
}

/**
 * Create a string option
 */
inline SimpleOption<std::string> ofString(
    std::string key,
    std::string defaultValue,
    SimpleOption<std::string>::ChangeCallback callback = nullptr
) {
    return SimpleOption<std::string>(
        std::move(key),
        std::make_shared<StringCodec>(),
        std::move(defaultValue),
        nullptr,
        std::move(callback)
    );
}

/**
 * Create an enum option
 */
template<typename E>
inline SimpleOption<E> ofEnum(
    std::string key,
    E defaultValue,
    typename SimpleOption<E>::ChangeCallback callback = nullptr
) {
    return SimpleOption<E>(
        std::move(key),
        std::make_shared<EnumCodec<E>>(),
        defaultValue,
        nullptr,
        std::move(callback)
    );
}

} // namespace VoxelEngine

// ============================================================================
// fmt formatter support for spdlog
// ============================================================================
namespace fmt {
    template<typename T>
    struct formatter<VoxelEngine::SimpleOption<T>> : formatter<T> {
        template<typename FormatContext>
        auto format(const VoxelEngine::SimpleOption<T>& opt, FormatContext& ctx) const {
            return formatter<T>::format(opt.getValue(), ctx);
        }
    };
}