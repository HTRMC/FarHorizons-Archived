#pragma once
#include <string>
#include <vector>
#include <utility>

namespace VoxelEngine {

// Property defines a blockstate property (like "type" for slabs)
// T is the enum type (e.g., SlabType)
template<typename T>
class Property {
public:
    std::string name;
    std::vector<std::pair<std::string, T>> values;

    Property(const std::string& n, std::initializer_list<std::pair<std::string, T>> vals)
        : name(n), values(vals) {}

    size_t getNumValues() const {
        return values.size();
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