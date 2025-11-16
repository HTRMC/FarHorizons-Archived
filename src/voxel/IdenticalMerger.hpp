#pragma once

#include "IndexMerger.hpp"
#include <vector>

namespace FarHorizon {

// IdenticalMerger - used when two coordinate lists are identical
// (from Minecraft's IdenticalMerger.java)
class IdenticalMerger : public IndexMerger {
private:
    std::vector<double> coords_;

public:
    explicit IdenticalMerger(const std::vector<double>& coords)
        : coords_(coords) {}

    const std::vector<double>& getList() const override {
        return coords_;
    }

    bool forMergedIndexes(const IndexConsumer& consumer) const override {
        int size = static_cast<int>(coords_.size()) - 1;

        for (int i = 0; i < size; i++) {
            if (!consumer(i, i, i)) {
                return false;
            }
        }

        return true;
    }

    int size() const override {
        return static_cast<int>(coords_.size());
    }
};

} // namespace FarHorizon