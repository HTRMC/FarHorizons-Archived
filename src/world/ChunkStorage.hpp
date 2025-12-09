#pragma once

#include "ChunkData.hpp"
#include <shared_mutex>
#include <array>
#include <unordered_map>
#include <vector>
#include <functional>

namespace FarHorizon {

/**
 * High-performance sharded chunk storage.
 *
 * Design principles:
 * - 64 independent shards minimize lock contention
 * - shared_mutex allows unlimited concurrent readers per shard
 * - Readers NEVER block readers (even on same shard)
 * - shared_ptr ensures safe access after lock release
 *
 * Performance characteristics:
 * - Lookup: ~50ns (shared lock + hash lookup)
 * - Insert: ~100ns (exclusive lock on one shard only)
 * - Delete: ~100ns (exclusive lock on one shard only)
 * - Concurrent reads: unlimited parallelism
 */
class ChunkStorage {
public:
    static constexpr size_t NUM_SHARDS = 64;

    ChunkStorage() = default;
    ~ChunkStorage() = default;

    // Non-copyable, non-movable (contains mutexes)
    ChunkStorage(const ChunkStorage&) = delete;
    ChunkStorage& operator=(const ChunkStorage&) = delete;

    /**
     * Get chunk data. Returns nullptr if not found.
     * Thread-safe, lock-free after shared_ptr copy.
     */
    ChunkDataPtr get(const ChunkPosition& pos) const;

    /**
     * Get chunk + all 6 face neighbors in one operation.
     * More efficient than 7 separate calls (fewer lock acquisitions).
     * Returns array: [center, west, east, down, up, north, south]
     * Null entries for missing chunks.
     */
    std::array<ChunkDataPtr, 7> getWithNeighbors(const ChunkPosition& pos) const;

    /**
     * Insert or replace chunk data.
     * Thread-safe, only locks one shard.
     * @return Previous data at position (nullptr if new)
     */
    ChunkDataPtr insert(const ChunkPosition& pos, ChunkDataPtr data);

    /**
     * Atomically update chunk data using compare-and-swap semantics.
     * Only updates if current data matches expected version.
     * @return true if update succeeded, false if version mismatch
     */
    bool compareAndSwap(const ChunkPosition& pos, uint32_t expectedVersion, ChunkDataPtr newData);

    /**
     * Remove chunk from storage.
     * @return The removed data (nullptr if didn't exist)
     */
    ChunkDataPtr remove(const ChunkPosition& pos);

    /**
     * Check if chunk exists (slightly faster than get() != nullptr).
     */
    bool contains(const ChunkPosition& pos) const;

    /**
     * Get all chunk positions (for iteration, unloading, etc.)
     * Returns a snapshot - safe to modify storage while iterating result.
     */
    std::vector<ChunkPosition> getAllPositions() const;

    /**
     * Get all chunks within distance of a position.
     * @param center Center position
     * @param radius Maximum distance (Euclidean)
     * @return Vector of (position, data) pairs
     */
    std::vector<std::pair<ChunkPosition, ChunkDataPtr>> getChunksInRadius(
        const ChunkPosition& center, float radius) const;

    /**
     * Remove all chunks outside radius from center.
     * @return Number of chunks removed
     */
    size_t removeOutsideRadius(const ChunkPosition& center, float radius);

    /**
     * Clear all chunks.
     */
    void clear();

    /**
     * Get total chunk count.
     */
    size_t size() const;

    /**
     * Apply a function to each chunk (read-only).
     * Function receives const ChunkDataPtr&.
     * Chunks may be added/removed during iteration (snapshot semantics per shard).
     */
    template<typename Func>
    void forEach(Func&& func) const {
        for (size_t i = 0; i < NUM_SHARDS; i++) {
            std::shared_lock lock(shards_[i].mutex);
            for (const auto& [pos, data] : shards_[i].chunks) {
                func(pos, data);
            }
        }
    }

private:
    struct Shard {
        mutable std::shared_mutex mutex;
        std::unordered_map<ChunkPosition, ChunkDataPtr, ChunkPositionHash> chunks;
    };

    std::array<Shard, NUM_SHARDS> shards_;

    // Fast shard selection using position hash
    size_t getShardIndex(const ChunkPosition& pos) const {
        return ChunkPositionHash{}(pos) % NUM_SHARDS;
    }

    Shard& getShard(const ChunkPosition& pos) {
        return shards_[getShardIndex(pos)];
    }

    const Shard& getShard(const ChunkPosition& pos) const {
        return shards_[getShardIndex(pos)];
    }
};

} // namespace FarHorizon