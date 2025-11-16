#pragma once

#include <vector>
#include <functional>

namespace FarHorizon {

// IndexMerger interface (from Minecraft's IndexMerger.java)
// Merges indices from two coordinate lists
class IndexMerger {
public:
    virtual ~IndexMerger() = default;

    // Get the merged coordinate list
    virtual const std::vector<double>& getList() const = 0;

    // Iterate through merged indices (returns false if consumer returns false)
    // Consumer receives: (firstIndex, secondIndex, resultIndex)
    using IndexConsumer = std::function<bool(int, int, int)>;
    virtual bool forMergedIndexes(const IndexConsumer& consumer) const = 0;

    // Get size of merged list
    virtual int size() const = 0;
};

} // namespace FarHorizon