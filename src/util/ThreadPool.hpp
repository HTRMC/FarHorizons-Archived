#pragma once

#include <concurrentqueue.h>
#include <thread>
#include <vector>
#include <atomic>
#include <functional>
#include <spdlog/spdlog.h>

namespace VoxelEngine {

/**
 * High-performance thread pool using lock-free concurrent queue
 * Based on cameron314/concurrentqueue for minimal contention
 */
class ThreadPool {
public:
    using Task = std::function<void()>;

    /**
     * Create a thread pool with the specified number of worker threads
     * @param numThreads Number of worker threads (default: hardware_concurrency)
     * @param name Name for logging purposes
     */
    ThreadPool(size_t numThreads = std::thread::hardware_concurrency(), const std::string& name = "ThreadPool")
        : m_name(name), m_running(true) {

        m_workers.reserve(numThreads);
        for (size_t i = 0; i < numThreads; i++) {
            m_workers.emplace_back(&ThreadPool::workerLoop, this);
        }

        spdlog::info("{} initialized with {} worker threads", m_name, numThreads);
    }

    ~ThreadPool() {
        shutdown();
    }

    /**
     * Enqueue a task to be executed by a worker thread
     * Lock-free and wait-free from the producer side
     */
    void enqueue(Task task) {
        m_taskQueue.enqueue(std::move(task));
        m_taskCount.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * Enqueue multiple tasks at once (bulk operation for better performance)
     */
    void enqueueBulk(std::vector<Task>&& tasks) {
        size_t count = tasks.size();
        m_taskQueue.enqueue_bulk(std::make_move_iterator(tasks.begin()), count);
        m_taskCount.fetch_add(count, std::memory_order_relaxed);
    }

    /**
     * Get approximate number of pending tasks
     */
    size_t getPendingTaskCount() const {
        return m_taskCount.load(std::memory_order_relaxed);
    }

    /**
     * Wait for all pending tasks to complete
     */
    void waitForCompletion() {
        while (m_taskCount.load(std::memory_order_acquire) > 0) {
            std::this_thread::yield();
        }
    }

    /**
     * Shutdown the thread pool and join all worker threads
     */
    void shutdown() {
        if (!m_running.exchange(false)) {
            return;  // Already shut down
        }

        // Wait for all tasks to complete
        waitForCompletion();

        // Join all worker threads
        for (auto& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        spdlog::info("{} shut down", m_name);
    }

    size_t getThreadCount() const {
        return m_workers.size();
    }

private:
    void workerLoop() {
        Task task;

        while (m_running.load(std::memory_order_relaxed)) {
            // Try to dequeue a task (lock-free)
            if (m_taskQueue.try_dequeue(task)) {
                // Execute the task
                task();
                m_taskCount.fetch_sub(1, std::memory_order_release);
            } else {
                // No tasks available, yield to avoid busy-waiting
                std::this_thread::yield();
            }
        }

        // Process remaining tasks before exiting
        while (m_taskQueue.try_dequeue(task)) {
            task();
            m_taskCount.fetch_sub(1, std::memory_order_release);
        }
    }

private:
    std::string m_name;
    std::vector<std::thread> m_workers;
    moodycamel::ConcurrentQueue<Task> m_taskQueue;
    std::atomic<size_t> m_taskCount{0};
    std::atomic<bool> m_running{true};
};

} // namespace VoxelEngine