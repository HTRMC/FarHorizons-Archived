#pragma once
#include <simdjson.h>
#include <functional>
#include <optional>
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <spdlog/spdlog.h>

namespace VoxelEngine {

template<typename T>
class DecodeResult {
public:
    std::optional<T> value;
    std::string error;

    static DecodeResult success(T val) {
        DecodeResult result;
        result.value = std::move(val);
        return result;
    }

    static DecodeResult failure(const std::string& err) {
        DecodeResult result;
        result.error = err;
        return result;
    }

    bool isSuccess() const { return value.has_value(); }
    bool isError() const { return !value.has_value(); }
};

template<typename T>
class Codec {
public:
    using DecodeFunc = std::function<DecodeResult<T>(simdjson::ondemand::value)>;

    explicit Codec(DecodeFunc decoder) : m_decoder(std::move(decoder)) {}

    DecodeResult<T> decode(simdjson::ondemand::value json) const {
        return m_decoder(json);
    }

    template<typename U>
    Codec<U> map(std::function<U(T)> mapper) const {
        return Codec<U>([decoder = m_decoder, mapper](simdjson::ondemand::value json) -> DecodeResult<U> {
            auto result = decoder(json);
            if (result.isError()) {
                return DecodeResult<U>::failure(result.error);
            }
            return DecodeResult<U>::success(mapper(result.value.value()));
        });
    }

    Codec<std::optional<T>> optional() const {
        return Codec<std::optional<T>>([decoder = m_decoder](simdjson::ondemand::value json) -> DecodeResult<std::optional<T>> {
            auto result = decoder(json);
            if (result.isError()) {
                return DecodeResult<std::optional<T>>::success(std::nullopt);
            }
            return DecodeResult<std::optional<T>>::success(result.value);
        });
    }

    Codec<T> withDefault(T defaultValue) const {
        return Codec<T>([decoder = m_decoder, defaultValue](simdjson::ondemand::value json) -> DecodeResult<T> {
            auto result = decoder(json);
            if (result.isError()) {
                return DecodeResult<T>::success(defaultValue);
            }
            return result;
        });
    }

private:
    DecodeFunc m_decoder;
};

class Codecs {
public:
    static Codec<double> DOUBLE() {
        return Codec<double>([](simdjson::ondemand::value json) -> DecodeResult<double> {
            double val;
            auto error = json.get_double().get(val);
            if (error) {
                return DecodeResult<double>::failure("Expected double");
            }
            return DecodeResult<double>::success(val);
        });
    }

    static Codec<float> FLOAT() {
        return Codec<float>([](simdjson::ondemand::value json) -> DecodeResult<float> {
            double val;
            auto error = json.get_double().get(val);
            if (error) {
                return DecodeResult<float>::failure("Expected double");
            }
            return DecodeResult<float>::success(static_cast<float>(val));
        });
    }

    static Codec<int64_t> INT64() {
        return Codec<int64_t>([](simdjson::ondemand::value json) -> DecodeResult<int64_t> {
            int64_t val;
            auto error = json.get_int64().get(val);
            if (error) {
                return DecodeResult<int64_t>::failure("Expected int64");
            }
            return DecodeResult<int64_t>::success(val);
        });
    }

    static Codec<int32_t> INT32() {
        return Codec<int32_t>([](simdjson::ondemand::value json) -> DecodeResult<int32_t> {
            int64_t val;
            auto error = json.get_int64().get(val);
            if (error) {
                return DecodeResult<int32_t>::failure("Expected int64");
            }
            return DecodeResult<int32_t>::success(static_cast<int32_t>(val));
        });
    }

    static Codec<std::string> STRING() {
        return Codec<std::string>([](simdjson::ondemand::value json) -> DecodeResult<std::string> {
            std::string_view val;
            auto error = json.get_string().get(val);
            if (error) {
                return DecodeResult<std::string>::failure("Expected string");
            }
            return DecodeResult<std::string>::success(std::string(val));
        });
    }

    static Codec<bool> BOOL() {
        return Codec<bool>([](simdjson::ondemand::value json) -> DecodeResult<bool> {
            bool val;
            auto error = json.get_bool().get(val);
            if (error) {
                return DecodeResult<bool>::failure("Expected bool");
            }
            return DecodeResult<bool>::success(val);
        });
    }

    template<typename T>
    static Codec<std::vector<T>> list(const Codec<T>& elementCodec) {
        return Codec<std::vector<T>>([elementCodec](simdjson::ondemand::value json) -> DecodeResult<std::vector<T>> {
            simdjson::ondemand::array arr;
            auto error = json.get_array().get(arr);
            if (error) {
                return DecodeResult<std::vector<T>>::failure("Expected array");
            }

            std::vector<T> result;
            for (auto element : arr) {
                auto decoded = elementCodec.decode(element.value());
                if (decoded.isError()) {
                    return DecodeResult<std::vector<T>>::failure("Array element decode failed: " + decoded.error);
                }
                result.push_back(std::move(decoded.value.value()));
            }
            return DecodeResult<std::vector<T>>::success(std::move(result));
        });
    }

    template<typename T>
    static Codec<std::map<std::string, T>> map(const Codec<T>& valueCodec) {
        return Codec<std::map<std::string, T>>([valueCodec](simdjson::ondemand::value json) -> DecodeResult<std::map<std::string, T>> {
            simdjson::ondemand::object obj;
            auto error = json.get_object().get(obj);
            if (error) {
                return DecodeResult<std::map<std::string, T>>::failure("Expected object");
            }

            std::map<std::string, T> result;
            for (auto field : obj) {
                std::string_view key_view;
                auto key_error = field.unescaped_key().get(key_view);
                if (key_error) {
                    return DecodeResult<std::map<std::string, T>>::failure("Failed to read map key");
                }

                auto decoded = valueCodec.decode(field.value());
                if (decoded.isError()) {
                    return DecodeResult<std::map<std::string, T>>::failure(
                        "Map value decode failed for key '" + std::string(key_view) + "': " + decoded.error
                    );
                }
                result[std::string(key_view)] = std::move(decoded.value.value());
            }
            return DecodeResult<std::map<std::string, T>>::success(std::move(result));
        });
    }
};

template<typename T>
class FieldCodec {
public:
    std::string fieldName;
    Codec<T> codec;
    std::optional<T> defaultValue;

    FieldCodec(std::string name, Codec<T> c)
        : fieldName(std::move(name)), codec(std::move(c)) {}

    FieldCodec(std::string name, Codec<T> c, T defVal)
        : fieldName(std::move(name)), codec(std::move(c)), defaultValue(std::move(defVal)) {}

    DecodeResult<T> decode(simdjson::ondemand::object& obj) const {
        simdjson::ondemand::value val;
        auto error = obj[fieldName].get(val);

        if (error) {
            if (defaultValue.has_value()) {
                return DecodeResult<T>::success(defaultValue.value());
            }
            return DecodeResult<T>::failure("Missing required field: " + fieldName);
        }

        auto result = codec.decode(val);
        if (result.isError()) {
            return DecodeResult<T>::failure("Field '" + fieldName + "': " + result.error);
        }
        return result;
    }
};

template<typename T>
FieldCodec<T> field(const std::string& name, const Codec<T>& codec) {
    return FieldCodec<T>(name, codec);
}

template<typename T>
FieldCodec<T> optionalField(const std::string& name, const Codec<T>& codec, T defaultValue) {
    return FieldCodec<T>(name, codec, std::move(defaultValue));
}

template<typename T>
class DispatchCodec {
public:
    using TypeGetter = std::function<std::string(const T&)>;
    using CodecGetter = std::function<Codec<T>(const std::string&)>;

    DispatchCodec(std::string typeField, CodecGetter codecGetter)
        : m_typeField(std::move(typeField)), m_codecGetter(std::move(codecGetter)) {}

    Codec<T> codec() const {
        return Codec<T>([typeField = m_typeField, codecGetter = m_codecGetter](simdjson::ondemand::value json) -> DecodeResult<T> {
            simdjson::ondemand::object obj;
            auto error = json.get_object().get(obj);
            if (error) {
                return DecodeResult<T>::failure("Expected object for dispatch");
            }

            simdjson::ondemand::value typeVal;
            error = obj[typeField].get(typeVal);
            if (error) {
                return DecodeResult<T>::failure("Missing type field: " + typeField);
            }

            std::string_view typeView;
            error = typeVal.get_string().get(typeView);
            if (error) {
                return DecodeResult<T>::failure("Type field must be string");
            }

            std::string type(typeView);
            auto specificCodec = codecGetter(type);

            return specificCodec.decode(json);
        });
    }

private:
    std::string m_typeField;
    CodecGetter m_codecGetter;
};

template<typename T>
DispatchCodec<T> dispatch(const std::string& typeField,
                          std::function<Codec<T>(const std::string&)> codecGetter) {
    return DispatchCodec<T>(typeField, std::move(codecGetter));
}

} // namespace VoxelEngine