#pragma once

#include <string>
#include <functional>
#include <optional>
#include <type_traits>
#include <limits>
#include <simdjson.h>
#include <spdlog/spdlog.h>
#include <magic_enum/magic_enum.hpp>

namespace VoxelEngine {

/**
 * Minecraft-style flexible option system with validation, serialization, and callbacks.
 * Uses compile-time polymorphism - no virtual functions or runtime overhead.
 */
template<typename T>
class SimpleOption {
public:
    using ChangeCallback = std::function<void(const T&)>;
    using Validator = std::function<std::optional<T>(const T&)>;

    /**
     * Create a simple option
     * @param key Translation key (e.g., "options.fov")
     * @param defaultValue Default value
     * @param validator Optional validator function
     * @param changeCallback Called when value changes
     */
    SimpleOption(
        std::string key,
        T defaultValue,
        Validator validator = nullptr,
        ChangeCallback changeCallback = nullptr
    ) : m_key(std::move(key)),
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

    // Serialize to JSON string (compile-time dispatch based on type)
    std::string serialize() const {
        if constexpr (std::is_same_v<T, bool>) {
            return m_value ? "true" : "false";
        } else if constexpr (std::is_integral_v<T>) {
            return std::to_string(m_value);
        } else if constexpr (std::is_floating_point_v<T>) {
            return std::to_string(m_value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return "\"" + m_value + "\"";
        } else if constexpr (std::is_enum_v<T>) {
            auto name = magic_enum::enum_name(m_value);
            return "\"" + std::string(name) + "\"";
        } else {
            static_assert(sizeof(T) == 0, "Unsupported type for SimpleOption");
        }
    }

    // Deserialize from simdjson (works with both ondemand::value and dom::element)
    template<typename JsonType>
    bool deserialize(JsonType& jsonValue) {
        std::optional<T> result;

        try {
            if constexpr (std::is_same_v<T, bool>) {
                result = jsonValue.get_bool();
            } else if constexpr (std::is_same_v<T, int32_t>) {
                int64_t val = jsonValue.get_int64();
                result = static_cast<int32_t>(val);
            } else if constexpr (std::is_same_v<T, float>) {
                double val = jsonValue.get_double();
                result = static_cast<float>(val);
            } else if constexpr (std::is_same_v<T, double>) {
                result = jsonValue.get_double();
            } else if constexpr (std::is_same_v<T, std::string>) {
                std::string_view sv = jsonValue.get_string();
                result = std::string(sv);
            } else if constexpr (std::is_enum_v<T>) {
                std::string_view sv = jsonValue.get_string();
                result = magic_enum::enum_cast<T>(sv);
            } else {
                static_assert(sizeof(T) == 0, "Unsupported type for SimpleOption");
            }
        } catch (...) {
            return false;
        }

        if (result.has_value()) {
            setValue(*result);
            return true;
        }
        return false;
    }

private:
    std::string m_key;
    T m_defaultValue;
    T m_value;
    Validator m_validator;
    ChangeCallback m_changeCallback;
};

// ============================================================================
// Factory functions for common option types with built-in validation
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