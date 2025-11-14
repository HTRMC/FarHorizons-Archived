#pragma once

#include <vector>
#include <memory>

namespace FarHorizon {

// Offset double list wrapper (from Minecraft's OffsetDoubleList.java)
// Wraps a vector of doubles and adds an offset to each value when accessed
class OffsetDoubleList {
private:
    std::shared_ptr<std::vector<double>> delegate_;
    double offset_;

public:
    // Constructor (OffsetDoubleList.java line 10)
    OffsetDoubleList(std::shared_ptr<std::vector<double>> delegate, double offset)
        : delegate_(delegate), offset_(offset) {}

    // Constructor that takes a const reference and makes a shared copy
    OffsetDoubleList(const std::vector<double>& delegate, double offset)
        : delegate_(std::make_shared<std::vector<double>>(delegate)), offset_(offset) {}

    // Get value at index with offset applied (OffsetDoubleList.java line 15)
    double operator[](size_t index) const {
        return (*delegate_)[index] + offset_;
    }

    // Get value at index (alternative accessor)
    double at(size_t index) const {
        return delegate_->at(index) + offset_;
    }

    // Get first element (front)
    double front() const {
        return delegate_->front() + offset_;
    }

    // Get last element (back)
    double back() const {
        return delegate_->back() + offset_;
    }

    // Size (OffsetDoubleList.java line 19)
    size_t size() const {
        return delegate_->size();
    }

    // Get underlying vector (for compatibility)
    const std::vector<double>& getDelegate() const {
        return *delegate_;
    }

    // Get offset
    double getOffset() const {
        return offset_;
    }

    // Create a materialized vector with offset applied
    std::vector<double> materialize() const {
        std::vector<double> result;
        result.reserve(delegate_->size());
        for (double val : *delegate_) {
            result.push_back(val + offset_);
        }
        return result;
    }
};

} // namespace FarHorizon