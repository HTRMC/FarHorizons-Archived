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
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::move(event));
    }

    // Get all events for processing on game thread
    std::vector<InputEvent> pollEvents() {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::vector<InputEvent> events;
        events.reserve(m_queue.size());

        while (!m_queue.empty()) {
            events.push_back(std::move(m_queue.front()));
            m_queue.pop();
        }

        return events;
    }

    // Check if queue is empty (thread-safe)
    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    // Get queue size (thread-safe)
    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    // Clear all events
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_queue.empty()) {
            m_queue.pop();
        }
    }

private:
    mutable std::mutex m_mutex;
    std::queue<InputEvent> m_queue;
};

} // namespace FarHorizon