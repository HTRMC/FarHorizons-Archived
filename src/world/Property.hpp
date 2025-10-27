#pragma once
#include <string>
#include <vector>
#include <utility>
#include <optional>

namespace VoxelEngine {

// Base class for type-erased property access
class PropertyBase {
public:
    std::string name;

    PropertyBase(const std::string& n) : name(n) {}
    virtual ~PropertyBase() = default;

    virtual size_t getNumValues() const = 0;
    virtual std::optional<size_t> getValueIndexByName(const std::string& valueName) const = 0;
};

// Property defines a blockstate property (like "type" for slabs)
// T is the enum type (e.g., SlabType)
template<typename T>
class Property : public PropertyBase {
public:
    std::vector<std::pair<std::string, T>> values;

    Property(const std::string& n, std::initializer_list<std::pair<std::string, T>> vals)
        : PropertyBase(n), values(vals) {}

    size_t getNumValues() const override {
        return values.size();
    }

    std::optional<size_t> getValueIndexByName(const std::string& valueName) const override {
        for (size_t i = 0; i < values.size(); i++) {
            if (values[i].first == valueName) {
                return i;
            }
        }
        return std::nullopt;
    }

    const std::string& getValueName(T value) const {
        for (const auto& [name, val] : values) {
            if (val == value) {
                return name;
            }
        }
        static const std::string empty = "";
        return empty;
    }

    T getValueByName(const std::string& name) const {
        for (const auto& [valueName, val] : values) {
            if (valueName == name) {
                return val;
            }
        }
        return values[0].second; // Return first value as default
    }

    int getValueIndex(T value) const {
        for (size_t i = 0; i < values.size(); i++) {
            if (values[i].second == value) {
                return static_cast<int>(i);
            }
        }
        return 0;
    }
};

} // namespace VoxelEngine