#include "ChunkStorage.hpp"
#include <tracy/Tracy.hpp>

namespace FarHorizon {

ChunkDataPtr ChunkStorage::get(const ChunkPosition& pos) const {
    const Shard& shard = getShard(pos);
    std::shared_lock lock(shard.mutex);

    auto it = shard.chunks.find(pos);
    if (it != shard.chunks.end()) {
        return it->second;  // Copy shared_ptr (atomic refcount increment)
    }
    return nullptr;
}

std::array<ChunkDataPtr, 7> ChunkStorage::getWithNeighbors(const ChunkPosition& pos) const {
    ZoneScoped;

    std::array<ChunkDataPtr, 7> result{};

    // Positions: center, west, east, down, up, north, south
    std::array<ChunkPosition, 7> positions = {{
        pos,
        {pos.x - 1, pos.y, pos.z},      // West
        {pos.x + 1, pos.y, pos.z},      // East
        {pos.x, pos.y - 1, pos.z},      // Down
        {pos.x, pos.y + 1, pos.z},      // Up
        {pos.x, pos.y, pos.z - 1},      // North
        {pos.x, pos.y, pos.z + 1}       // South
    }};

    // Group positions by shard to minimize lock acquisitions
    // For 7 chunks, worst case is 7 different shards, best case is 1
    // On average, with 64 shards, we'll hit ~7 different shards

    // Simple approach: just do 7 lookups with RAII locks
    // The shared_lock is extremely fast (~20ns) so this is fine
    for (size_t i = 0; i < 7; i++) {
        result[i] = get(positions[i]);
    }

    return result;
}

ChunkDataPtr ChunkStorage::insert(const ChunkPosition& pos, ChunkDataPtr data) {
    ZoneScoped;

    Shard& shard = getShard(pos);
    std::unique_lock lock(shard.mutex);

    auto it = shard.chunks.find(pos);
    ChunkDataPtr previous = nullptr;

    if (it != shard.chunks.end()) {
        previous = std::move(it->second);
        it->second = std::move(data);
    } else {
        shard.chunks.emplace(pos, std::move(data));
    }

    return previous;
}

bool ChunkStorage::compareAndSwap(const ChunkPosition& pos, uint32_t expectedVersion, ChunkDataPtr newData) {
    ZoneScoped;

    Shard& shard = getShard(pos);
    std::unique_lock lock(shard.mutex);

    auto it = shard.chunks.find(pos);
    if (it == shard.chunks.end()) {
        // Chunk doesn't exist - only succeed if expectedVersion is 0 (new chunk)
        if (expectedVersion == 0) {
            shard.chunks.emplace(pos, std::move(newData));
            return true;
        }
        return false;
    }

    // Check version matches
    if (it->second->getVersion() != expectedVersion) {
        return false;  // Version mismatch - concurrent modification
    }

    it->second = std::move(newData);
    return true;
}

ChunkDataPtr ChunkStorage::remove(const ChunkPosition& pos) {
    ZoneScoped;

    Shard& shard = getShard(pos);
    std::unique_lock lock(shard.mutex);

    auto it = shard.chunks.find(pos);
    if (it != shard.chunks.end()) {
        ChunkDataPtr data = std::move(it->second);
        shard.chunks.erase(it);
        return data;
    }
    return nullptr;
}

bool ChunkStorage::contains(const ChunkPosition& pos) const {
    const Shard& shard = getShard(pos);
    std::shared_lock lock(shard.mutex);
    return shard.chunks.find(pos) != shard.chunks.end();
}

std::vector<ChunkPosition> ChunkStorage::getAllPositions() const {
    ZoneScoped;

    std::vector<ChunkPosition> positions;

    // Pre-reserve approximate size
    size_t totalSize = 0;
    for (const auto& shard : shards_) {
        std::shared_lock lock(shard.mutex);
        totalSize += shard.chunks.size();
    }
    positions.reserve(totalSize);

    // Collect all positions
    for (const auto& shard : shards_) {
        std::shared_lock lock(shard.mutex);
        for (const auto& [pos, data] : shard.chunks) {
            positions.push_back(pos);
        }
    }

    return positions;
}

std::vector<std::pair<ChunkPosition, ChunkDataPtr>> ChunkStorage::getChunksInRadius(
    const ChunkPosition& center, float radius) const {
    ZoneScoped;

    std::vector<std::pair<ChunkPosition, ChunkDataPtr>> result;

    for (const auto& shard : shards_) {
        std::shared_lock lock(shard.mutex);
        for (const auto& [pos, data] : shard.chunks) {
            if (pos.distanceTo(center) <= radius) {
                result.emplace_back(pos, data);
            }
        }
    }

    return result;
}

size_t ChunkStorage::removeOutsideRadius(const ChunkPosition& center, float radius) {
    ZoneScoped;

    size_t removedCount = 0;

    for (auto& shard : shards_) {
        std::unique_lock lock(shard.mutex);

        auto it = shard.chunks.begin();
        while (it != shard.chunks.end()) {
            if (it->first.distanceTo(center) > radius) {
                it = shard.chunks.erase(it);
                removedCount++;
            } else {
                ++it;
            }
        }
    }

    return removedCount;
}

void ChunkStorage::clear() {
    ZoneScoped;

    for (auto& shard : shards_) {
        std::unique_lock lock(shard.mutex);
        shard.chunks.clear();
    }
}

size_t ChunkStorage::size() const {
    size_t total = 0;
    for (const auto& shard : shards_) {
        std::shared_lock lock(shard.mutex);
        total += shard.chunks.size();
    }
    return total;
}

} // namespace FarHorizon