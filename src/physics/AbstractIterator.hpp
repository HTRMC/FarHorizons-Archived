#pragma once

#include <optional>
#include <stdexcept>

namespace FarHorizon {

// AbstractIterator base class (similar to Guava's AbstractIterator)
// Provides hasNext/next boilerplate, subclasses implement computeNext()
template<typename T>
class AbstractIterator {
public:
    virtual ~AbstractIterator() = default;

    // Check if there are more elements (AbstractIterator: public boolean hasNext())
    bool hasNext() {
        if (!nextElement_.has_value()) {
            nextElement_ = computeNext();
        }
        return nextElement_.has_value();
    }

    // Get the next element (AbstractIterator: public T next())
    T next() {
        if (!hasNext()) {
            throw std::runtime_error("AbstractIterator::next() called but no next element");
        }

        T result = nextElement_.value();
        nextElement_ = std::nullopt;
        return result;
    }

protected:
    // Compute the next element (AbstractIterator: protected abstract T computeNext())
    // Return std::nullopt to signal end of iteration (equivalent to endOfData() in Java)
    virtual std::optional<T> computeNext() = 0;

private:
    std::optional<T> nextElement_;
};

} // namespace FarHorizon