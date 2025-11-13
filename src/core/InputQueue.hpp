#pragma once

#include "InputEvent.hpp"
#include <queue>
#include <mutex>
#include <vector>

namespace FarHorizon {

// Thread-safe input event queue
class InputQueue {
public:
    InputQueue() = default;
    ~InputQueue() = default;

    // Prevent copying
    InputQueue(const InputQueue&) = delete;
    InputQueue& operator=(const InputQueue&) = delete;

    // Push event from input thread (callbacks)
    void push(InputEvent&& event) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(event));
    }

    // Get all events for processing on game thread
    std::vector<InputEvent> pollEvents() {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<InputEvent> events;
        events.reserve(queue_.size());

        while (!queue_.empty()) {
            events.push_back(std::move(queue_.front()));
            queue_.pop();
        }

        return events;
    }

    // Check if queue is empty (thread-safe)
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    // Get queue size (thread-safe)
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    // Clear all events
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
    }

private:
    mutable std::mutex mutex_;
    std::queue<InputEvent> queue_;
};

} // namespace FarHorizon