#include "MouseCapture.hpp"
#include <spdlog/spdlog.h>

namespace FarHorizon {

MouseCapture::MouseCapture(GLFWwindow* window)
    : window_(window) {
    // Get initial window dimensions
    glfwGetWindowSize(window, &windowWidth_, &windowHeight_);

    // Get initial cursor position
    glfwGetCursorPos(window, &cursorX_, &cursorY_);

    spdlog::debug("MouseCapture initialized ({}x{})", windowWidth_, windowHeight_);
}

void MouseCapture::lockCursor() {
    // Only lock if window is focused
    if (!windowFocused_) {
        spdlog::debug("Cannot lock cursor: window not focused");
        return;
    }

    if (!cursorLocked_) {
        cursorLocked_ = true;

        // Center cursor position
        centerCursor();

        // Set GLFW cursor mode to disabled (hidden and locked)
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // Enable raw mouse input if supported and enabled
        if (rawMouseInput_ && isRawMouseInputSupported()) {
            glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }

        // Mark resolution changed to skip first delta (prevents spike)
        resolutionChanged_ = true;

        // Reset deltas
        cursorDeltaX_ = 0.0;
        cursorDeltaY_ = 0.0;

        spdlog::debug("Cursor locked");
        notifyStateChange();
    }
}

void MouseCapture::unlockCursor() {
    if (cursorLocked_) {
        cursorLocked_ = false;

        // Center cursor before unlocking
        centerCursor();

        // Set GLFW cursor mode to normal (visible and free)
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        // Disable raw mouse input
        if (isRawMouseInputSupported()) {
            glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        }

        // Reset deltas
        cursorDeltaX_ = 0.0;
        cursorDeltaY_ = 0.0;

        spdlog::debug("Cursor unlocked");
        notifyStateChange();
    }
}

void MouseCapture::onWindowFocusChanged(bool focused) {
    windowFocused_ = focused;

    if (!focused && cursorLocked_) {
        // Unlock cursor when window loses focus
        spdlog::debug("Window lost focus, unlocking cursor");
        unlockCursor();
    }
}

void MouseCapture::onWindowResized(int width, int height) {
    windowWidth_ = width;
    windowHeight_ = height;

    // Mark resolution changed to prevent delta spike
    if (cursorLocked_) {
        resolutionChanged_ = true;
        centerCursor();
    }

    spdlog::debug("Window resized: {}x{}", width, height);
}

void MouseCapture::updateCursorPosition(double x, double y) {
    if (resolutionChanged_) {
        // Skip first update after resolution change to prevent delta spike
        cursorX_ = x;
        cursorY_ = y;
        cursorDeltaX_ = 0.0;
        cursorDeltaY_ = 0.0;
        resolutionChanged_ = false;
    } else {
        // Calculate delta only if window is focused
        if (windowFocused_ && cursorLocked_) {
            cursorDeltaX_ += (x - cursorX_);
            cursorDeltaY_ += (y - cursorY_);
        }

        cursorX_ = x;
        cursorY_ = y;
    }
}

void MouseCapture::resetDeltas() {
    cursorDeltaX_ = 0.0;
    cursorDeltaY_ = 0.0;
}

void MouseCapture::centerCursor() {
    if (windowWidth_ > 0 && windowHeight_ > 0) {
        cursorX_ = windowWidth_ / 2.0;
        cursorY_ = windowHeight_ / 2.0;
        glfwSetCursorPos(window_, cursorX_, cursorY_);
    }
}

void MouseCapture::notifyStateChange() {
    if (cursorStateCallback_) {
        cursorStateCallback_(cursorLocked_);
    }
}

bool MouseCapture::isRawMouseInputSupported() {
    // Check if GLFW supports raw mouse motion
    return glfwRawMouseMotionSupported() == GLFW_TRUE;
}

void MouseCapture::setRawMouseInput(bool enabled) {
    rawMouseInput_ = enabled;

    // Apply immediately if cursor is locked
    if (cursorLocked_ && isRawMouseInputSupported()) {
        glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION,
                        enabled ? GLFW_TRUE : GLFW_FALSE);
        spdlog::debug("Raw mouse input: {}", enabled ? "enabled" : "disabled");
    }
}

} // namespace FarHorizon
