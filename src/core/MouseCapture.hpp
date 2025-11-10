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
    bool isCursorLocked() const { return m_cursorLocked; }

    // Window state handling
    void onWindowFocusChanged(bool focused);
    void onWindowResized(int width, int height);

    // Mouse position tracking (with resolution change protection)
    void updateCursorPosition(double x, double y);
    double getCursorDeltaX() const { return m_cursorDeltaX; }
    double getCursorDeltaY() const { return m_cursorDeltaY; }
    void resetDeltas();

    // Mark that resolution changed (prevents delta spike on next update)
    void markResolutionChanged() { m_resolutionChanged = true; }

    // Get current cursor position
    double getCursorX() const { return m_cursorX; }
    double getCursorY() const { return m_cursorY; }

    // Callback for cursor state changes
    void setCursorStateCallback(CursorStateCallback callback) {
        m_cursorStateCallback = callback;
    }

    // Check if raw mouse input is supported
    static bool isRawMouseInputSupported();

    // Enable/disable raw mouse input (bypasses OS acceleration)
    void setRawMouseInput(bool enabled);
    bool isRawMouseInputEnabled() const { return m_rawMouseInput; }

private:
    void centerCursor();
    void notifyStateChange();

private:
    GLFWwindow* m_window;

    // Cursor state
    bool m_cursorLocked = false;
    bool m_windowFocused = true;
    bool m_resolutionChanged = false;
    bool m_rawMouseInput = false;

    // Cursor position tracking
    double m_cursorX = 0.0;
    double m_cursorY = 0.0;
    double m_cursorDeltaX = 0.0;
    double m_cursorDeltaY = 0.0;

    // Window dimensions (for centering)
    int m_windowWidth = 0;
    int m_windowHeight = 0;

    // Callback
    CursorStateCallback m_cursorStateCallback;
};

} // namespace FarHorizon
