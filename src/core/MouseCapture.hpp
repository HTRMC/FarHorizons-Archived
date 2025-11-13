#pragma once

#include <GLFW/glfw3.h>
#include <functional>

namespace FarHorizon {

/**
 * Mouse Capture System - Controls cursor locking/unlocking
 *
 * Manages the transition between:
 * - Locked (in-game): Cursor hidden, captured for camera control
 * - Unlocked (menus): Cursor visible, normal OS behavior
 *
 * Features:
 * - Automatic lock/unlock based on game state
 * - Window focus handling
 * - Resolution change protection (prevents delta spikes)
 * - Smooth transitions
 */
class MouseCapture {
public:
    using CursorStateCallback = std::function<void(bool locked)>;

    explicit MouseCapture(GLFWwindow* window);
    ~MouseCapture() = default;

    // Cursor control
    void lockCursor();
    void unlockCursor();
    bool isCursorLocked() const { return cursorLocked_; }

    // Window state handling
    void onWindowFocusChanged(bool focused);
    void onWindowResized(int width, int height);

    // Mouse position tracking (with resolution change protection)
    void updateCursorPosition(double x, double y);
    double getCursorDeltaX() const { return cursorDeltaX_; }
    double getCursorDeltaY() const { return cursorDeltaY_; }
    void resetDeltas();

    // Mark that resolution changed (prevents delta spike on next update)
    void markResolutionChanged() { resolutionChanged_ = true; }

    // Get current cursor position
    double getCursorX() const { return cursorX_; }
    double getCursorY() const { return cursorY_; }

    // Callback for cursor state changes
    void setCursorStateCallback(CursorStateCallback callback) {
        cursorStateCallback_ = callback;
    }

    // Check if raw mouse input is supported
    static bool isRawMouseInputSupported();

    // Enable/disable raw mouse input (bypasses OS acceleration)
    void setRawMouseInput(bool enabled);
    bool isRawMouseInputEnabled() const { return rawMouseInput_; }

private:
    void centerCursor();
    void notifyStateChange();

private:
    GLFWwindow* window_;

    // Cursor state
    bool cursorLocked_ = false;
    bool windowFocused_ = true;
    bool resolutionChanged_ = false;
    bool rawMouseInput_ = false;

    // Cursor position tracking
    double cursorX_ = 0.0;
    double cursorY_ = 0.0;
    double cursorDeltaX_ = 0.0;
    double cursorDeltaY_ = 0.0;

    // Window dimensions (for centering)
    int windowWidth_ = 0;
    int windowHeight_ = 0;

    // Callback
    CursorStateCallback cursorStateCallback_;
};

} // namespace FarHorizon
