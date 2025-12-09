#pragma once

#include "BlockState.hpp"
#include "ChunkPalette.hpp"
#include "Chunk.hpp"
#include <glm/glm.hpp>
#include <array>
#include <memory>
#include <atomic>

namespace FarHorizon {

/**
 * Immutable chunk data - thread-safe for concurrent reads.
 *
 * Once created, ChunkData is NEVER modified. This enables:
 * - Lock-free reads from multiple mesh workers
 * - Safe concurrent access without synchronization
 * - Automatic cleanup via shared_ptr reference counting
 *
 * For edits, use withBlockState() to create a new ChunkData with the modification.
 */
class ChunkData {
public:
    // Create empty chunk at position
    explicit ChunkData(const ChunkPosition& position);

    // Create from existing data (used by generate() and withBlockState())
    ChunkData(const ChunkPosition& position,
              ChunkPalette palette,
              std::array<uint8_t, CHUNK_VOLUME> data,
              bool empty,
              uint32_t version);

    // Accessors - all const, thread-safe
    const ChunkPosition& getPosition() const { return position_; }
    BlockState getBlockState(uint32_t x, uint32_t y, uint32_t z) const;
    bool isEmpty() const { return empty_; }
    uint32_t getVersion() const { return version_; }
    const ChunkPalette& getPalette() const { return palette_; }
    const uint8_t* getData() const { return data_.data(); }

    /**
     * Create a NEW ChunkData with one block changed (copy-on-write).
     * The original ChunkData is unchanged.
     *
     * @return New ChunkData with the modification, incremented version
     */
    std::shared_ptr<const ChunkData> withBlockState(
        uint32_t x, uint32_t y, uint32_t z, BlockState state) const;

    /**
     * Generate terrain and return new immutable ChunkData.
     * This is a static factory method.
     */
    static std::shared_ptr<const ChunkData> generate(const ChunkPosition& position);

private:
    const ChunkPosition position_;
    const ChunkPalette palette_;
    const std::array<uint8_t, CHUNK_VOLUME> data_;
    const bool empty_;
    const uint32_t version_;  // Incremented on each edit for mesh invalidation

    static uint32_t getBlockIndex(uint32_t x, uint32_t y, uint32_t z) {
        return x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
    }
};

// Type alias for the standard way to hold chunk data
using ChunkDataPtr = std::shared_ptr<const ChunkData>;

} // namespace FarHorizon