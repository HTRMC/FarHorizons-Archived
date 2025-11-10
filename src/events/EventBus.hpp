#pragma once

#include "Event.hpp"
#include <vector>
#include <memory>
#include <queue>
#include <unordered_map>
#include <functional>

namespace FarHorizon {

// Event callback type
using EventCallback = std::function<void(Event&)>;

// Event listener handle for unsubscribing
using ListenerHandle = size_t;

// Event Bus for decoupling event producers from consumers
class EventBus {
public:
    // Subscribe to a specific event type
    template<typename T>
    static ListenerHandle subscribe(const std::function<void(T&)>& callback) {
        EventType type = T::getStaticType();
        ListenerHandle handle = s_nextHandle++;

        // Wrap the typed callback in a generic Event callback
        EventCallback wrappedCallback = [callback](Event& e) {
            callback(static_cast<T&>(e));
        };

        s_listeners[type].emplace_back(handle, wrappedCallback);
        return handle;
    }

    // Subscribe to all events
    static ListenerHandle subscribeAll(const EventCallback& callback) {
        ListenerHandle handle = s_nextHandle++;
        s_globalListeners.emplace_back(handle, callback);
        return handle;
    }

    // Unsubscribe using a handle
    static void unsubscribe(ListenerHandle handle) {
        // Remove from typed listeners
        for (auto& [type, listeners] : s_listeners) {
            listeners.erase(
                std::remove_if(listeners.begin(), listeners.end(),
                    [handle](const auto& pair) { return pair.first == handle; }),
                listeners.end()
            );
        }

        // Remove from global listeners
        s_globalListeners.erase(
            std::remove_if(s_globalListeners.begin(), s_globalListeners.end(),
                [handle](const auto& pair) { return pair.first == handle; }),
            s_globalListeners.end()
        );
    }

    // Post an event immediately (synchronous)
    static void post(Event& event) {
        // Notify global listeners
        for (auto& [handle, callback] : s_globalListeners) {
            callback(event);
            if (event.handled) break;
        }

        // Notify type-specific listeners
        auto it = s_listeners.find(event.getEventType());
        if (it != s_listeners.end()) {
            for (auto& [handle, callback] : it->second) {
                callback(event);
                if (event.handled) break;
            }
        }
    }

    // Queue an event for later processing (asynchronous)
    static void queue(std::unique_ptr<Event> event) {
        s_eventQueue.push(std::move(event));
    }

    // Process all queued events
    static void processQueue() {
        while (!s_eventQueue.empty()) {
            auto& event = s_eventQueue.front();
            post(*event);
            s_eventQueue.pop();
        }
    }

    // Clear all listeners
    static void clear() {
        s_listeners.clear();
        s_globalListeners.clear();
    }

    // Clear event queue
    static void clearQueue() {
        while (!s_eventQueue.empty()) {
            s_eventQueue.pop();
        }
    }

private:
    static inline ListenerHandle s_nextHandle = 0;
    static inline std::unordered_map<EventType, std::vector<std::pair<ListenerHandle, EventCallback>>> s_listeners;
    static inline std::vector<std::pair<ListenerHandle, EventCallback>> s_globalListeners;
    static inline std::queue<std::unique_ptr<Event>> s_eventQueue;
};

} // namespace FarHorizon