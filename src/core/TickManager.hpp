#pragma once

#include <chrono>

namespace FarHorizon {

// Minecraft's RenderTickCounter implementation (copied exactly from RenderTickCounter.Dynamic)
// Handles fixed 20 ticks/second with smooth sub-tick interpolation for rendering
class TickManager {
public:
    static constexpr float TICKS_PER_SECOND = 20.0f;
    static constexpr float TICK_TIME = 1000.0f / TICKS_PER_SECOND; // 50ms per tick

private:
    float dynamicDeltaTicks_ = 0.0f;  // Delta time in "ticks" (not seconds)
    float tickProgress_ = 0.0f;        // Accumulated fractional ticks (0.0 to 1.0+)
    double lastTimeMillis_ = 0.0;      // Previous frame time (DOUBLE to avoid precision loss!)
    double timeMillis_ = 0.0;          // Current time (DOUBLE to avoid precision loss!)
    bool paused_ = false;
    float tickProgressBeforePause_ = 0.0f;

public:
    TickManager() {
        // Initialize with zero (use relative time, not epoch time to avoid precision loss)
        lastTimeMillis_ = timeMillis_ = 0;
    }

    // Minecraft's beginRenderTick - returns number of ticks to run
    // Based on RenderTickCounter.Dynamic.beginRenderTick()
    int beginRenderTick(float deltaTimeSeconds, bool shouldTick) {
        // Convert delta time to milliseconds and add to current time
        // Keep as double to preserve fractional milliseconds (critical for high framerates!)
        double currentTimeMillis = timeMillis_ + (static_cast<double>(deltaTimeSeconds) * 1000.0);
        setTimeMillis(currentTimeMillis);

        if (!shouldTick) {
            return 0;
        }

        // Calculate delta in "tick units" (Minecraft's formula)
        // If 50ms passed and TICK_TIME=50ms, then dynamicDeltaTicks = 1.0
        dynamicDeltaTicks_ = static_cast<float>(currentTimeMillis - lastTimeMillis_) / TICK_TIME;
        lastTimeMillis_ = currentTimeMillis;

        // Accumulate fractional ticks (THIS IS THE KEY!)
        tickProgress_ += dynamicDeltaTicks_;

        // Extract integer ticks to run
        int ticksToRun = static_cast<int>(tickProgress_);

        // Keep fractional part for interpolation (0.0 to 1.0)
        tickProgress_ -= static_cast<float>(ticksToRun);

        // Cap at 10 ticks per frame (Minecraft's limit to prevent spiral of death)
        return std::min(ticksToRun, 10);
    }

    // Get partial tick for smooth interpolation (0.0 to 1.0)
    // Used to interpolate entity positions between ticks
    // Minecraft's getTickProgress()
    float getTickProgress(bool ignoreFreeze = false) const {
        if (paused_) {
            return tickProgressBeforePause_;
        }
        return tickProgress_;
    }

    // Pause/unpause (for menus)
    void setPaused(bool paused) {
        if (paused && !paused_) {
            tickProgressBeforePause_ = tickProgress_;
        } else if (!paused && paused_) {
            tickProgress_ = tickProgressBeforePause_;
        }
        paused_ = paused;
    }

    bool isPaused() const {
        return paused_;
    }

private:
    void setTimeMillis(double timeMillis) {
        timeMillis_ = timeMillis;
    }
};

} // namespace FarHorizon