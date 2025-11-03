#include "MouseCapture.hpp"
#include <spdlog/spdlog.h>

namespace VoxelEngine {

MouseCapture::MouseCapture(GLFWwindow* window)
    : m_window(window) {
    // Get initial window dimensions
    glfwGetWindowSize(window, &m_windowWidth, &m_windowHeight);

    // Get initial cursor position
    glfwGetCursorPos(window, &m_cursorX, &m_cursorY);

    spdlog::debug("MouseCapture initialized ({}x{})", m_windowWidth, m_windowHeight);
}

void MouseCapture::lockCursor() {
    // Only lock if window is focused
    if (!m_windowFocused) {
        spdlog::debug("Cannot lock cursor: window not focused");
        return;
    }

    if (!m_cursorLocked) {
        m_cursorLocked = true;

        // Center cursor position
        centerCursor();

        // Set GLFW cursor mode to disabled (hidden and locked)
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // Enable raw mouse input if supported and enabled
        if (m_rawMouseInput && isRawMouseInputSupported()) {
            glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }

        // Mark resolution changed to skip first delta (prevents spike)
        m_resolutionChanged = true;

        // Reset deltas
        m_cursorDeltaX = 0.0;
        m_cursorDeltaY = 0.0;

        spdlog::debug("Cursor locked");
        notifyStateChange();
    }
}

void MouseCapture::unlockCursor() {
    if (m_cursorLocked) {
        m_cursorLocked = false;

        // Center cursor before unlocking
        centerCursor();

        // Set GLFW cursor mode to normal (visible and free)
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        // Disable raw mouse input
        if (isRawMouseInputSupported()) {
            glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        }

        // Reset deltas
        m_cursorDeltaX = 0.0;
        m_cursorDeltaY = 0.0;

        spdlog::debug("Cursor unlocked");
        notifyStateChange();
    }
}

void MouseCapture::onWindowFocusChanged(bool focused) {
    m_windowFocused = focused;

    if (!focused && m_cursorLocked) {
        // Unlock cursor when window loses focus
        spdlog::debug("Window lost focus, unlocking cursor");
        unlockCursor();
    }
}

void MouseCapture::onWindowResized(int width, int height) {
    m_windowWidth = width;
    m_windowHeight = height;

    // Mark resolution changed to prevent delta spike
    if (m_cursorLocked) {
        m_resolutionChanged = true;
        centerCursor();
    }

    spdlog::debug("Window resized: {}x{}", width, height);
}

void MouseCapture::updateCursorPosition(double x, double y) {
    if (m_resolutionChanged) {
        // Skip first update after resolution change to prevent delta spike
        m_cursorX = x;
        m_cursorY = y;
        m_cursorDeltaX = 0.0;
        m_cursorDeltaY = 0.0;
        m_resolutionChanged = false;
    } else {
        // Calculate delta only if window is focused
        if (m_windowFocused && m_cursorLocked) {
            m_cursorDeltaX += (x - m_cursorX);
            m_cursorDeltaY += (y - m_cursorY);
        }

        m_cursorX = x;
        m_cursorY = y;
    }
}

void MouseCapture::resetDeltas() {
    m_cursorDeltaX = 0.0;
    m_cursorDeltaY = 0.0;
}

void MouseCapture::centerCursor() {
    if (m_windowWidth > 0 && m_windowHeight > 0) {
        m_cursorX = m_windowWidth / 2.0;
        m_cursorY = m_windowHeight / 2.0;
        glfwSetCursorPos(m_window, m_cursorX, m_cursorY);
    }
}

void MouseCapture::notifyStateChange() {
    if (m_cursorStateCallback) {
        m_cursorStateCallback(m_cursorLocked);
    }
}

bool MouseCapture::isRawMouseInputSupported() {
    // Check if GLFW supports raw mouse motion
    return glfwRawMouseMotionSupported() == GLFW_TRUE;
}

void MouseCapture::setRawMouseInput(bool enabled) {
    m_rawMouseInput = enabled;

    // Apply immediately if cursor is locked
    if (m_cursorLocked && isRawMouseInputSupported()) {
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION,
                        enabled ? GLFW_TRUE : GLFW_FALSE);
        spdlog::debug("Raw mouse input: {}", enabled ? "enabled" : "disabled");
    }
}

} // namespace VoxelEngine
