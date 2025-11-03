#include "Window.hpp"
#include "MouseCapture.hpp"
#include <stdexcept>
#include <spdlog/spdlog.h>

namespace VoxelEngine {

static bool s_GLFWInitialized = false;

static void glfwErrorCallback(int error, const char* description) {
    spdlog::error("GLFW Error ({}): {}", error, description);
}

Window::Window(const WindowProperties& props)
    : m_properties(props)
    , m_width(props.width)
    , m_height(props.height) {
    init();
}

Window::~Window() {
    shutdown();
}

Window::Window(Window&& other) noexcept
    : m_window(other.m_window)
    , m_properties(std::move(other.m_properties))
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_minimized(other.m_minimized)
    , m_focused(other.m_focused)
    , m_resizeCallback(std::move(other.m_resizeCallback))
    , m_closeCallback(std::move(other.m_closeCallback))
    , m_focusCallback(std::move(other.m_focusCallback)) {
    other.m_window = nullptr;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        shutdown();

        m_window = other.m_window;
        m_properties = std::move(other.m_properties);
        m_width = other.m_width;
        m_height = other.m_height;
        m_minimized = other.m_minimized;
        m_focused = other.m_focused;
        m_resizeCallback = std::move(other.m_resizeCallback);
        m_closeCallback = std::move(other.m_closeCallback);
        m_focusCallback = std::move(other.m_focusCallback);

        other.m_window = nullptr;
    }
    return *this;
}

void Window::init() {
    // Initialize GLFW
    if (!s_GLFWInitialized) {
        int success = glfwInit();
        if (!success) {
            throw std::runtime_error("Failed to initialize GLFW!");
        }
        glfwSetErrorCallback(glfwErrorCallback);
        s_GLFWInitialized = true;
    }

    // Configure GLFW for Vulkan
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, m_properties.resizable ? GLFW_TRUE : GLFW_FALSE);

    // Create window
    GLFWmonitor* monitor = m_properties.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    m_window = glfwCreateWindow(
        static_cast<int>(m_properties.width),
        static_cast<int>(m_properties.height),
        m_properties.title.c_str(),
        monitor,
        nullptr
    );

    if (!m_window) {
        throw std::runtime_error("Failed to create GLFW window!");
    }

    // Set user pointer for callbacks
    glfwSetWindowUserPointer(m_window, this);

    // Setup callbacks
    setupCallbacks();

    // Initialize mouse capture system
    m_mouseCapture = std::make_unique<MouseCapture>(m_window);

    spdlog::info("Window created: {}x{}", m_properties.width, m_properties.height);
}

void Window::shutdown() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
}

void Window::setupCallbacks() {
    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);
    glfwSetWindowCloseCallback(m_window, windowCloseCallback);
    glfwSetWindowFocusCallback(m_window, windowFocusCallback);
    glfwSetWindowIconifyCallback(m_window, windowIconifyCallback);
}

void Window::pollEvents() {
    glfwPollEvents();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(m_window);
}

void Window::close() {
    glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}

void Window::setTitle(const std::string& title) {
    m_properties.title = title;
    glfwSetWindowTitle(m_window, title.c_str());
}

void Window::setSize(uint32_t width, uint32_t height) {
    glfwSetWindowSize(m_window, static_cast<int>(width), static_cast<int>(height));
}

void Window::setVSync(bool enabled) {
    m_properties.vsync = enabled;
    // Note: VSync is typically handled by the swap chain in Vulkan
}

void Window::setFullscreen(bool fullscreen) {
    if (m_properties.fullscreen == fullscreen) {
        return;
    }

    m_properties.fullscreen = fullscreen;

    if (fullscreen) {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        glfwSetWindowMonitor(m_window, nullptr, 100, 100, m_properties.width, m_properties.height, 0);
    }
}

void Window::maximize() {
    glfwMaximizeWindow(m_window);
}

void Window::minimize() {
    glfwIconifyWindow(m_window);
}

void Window::restore() {
    glfwRestoreWindow(m_window);
}

void Window::setCursorMode(int mode) {
    glfwSetInputMode(m_window, GLFW_CURSOR, mode);
}

void Window::setCursorPos(double x, double y) {
    glfwSetCursorPos(m_window, x, y);
}

// Static callbacks
void Window::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));
    windowPtr->m_width = static_cast<uint32_t>(width);
    windowPtr->m_height = static_cast<uint32_t>(height);
    windowPtr->m_minimized = (width == 0 || height == 0);

    // Notify mouse capture system
    if (windowPtr->m_mouseCapture) {
        windowPtr->m_mouseCapture->onWindowResized(width, height);
    }

    if (windowPtr->m_resizeCallback && !windowPtr->m_minimized) {
        windowPtr->m_resizeCallback(windowPtr->m_width, windowPtr->m_height);
    }
}

void Window::windowCloseCallback(GLFWwindow* window) {
    auto* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (windowPtr->m_closeCallback) {
        windowPtr->m_closeCallback();
    }
}

void Window::windowFocusCallback(GLFWwindow* window, int focused) {
    auto* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));
    windowPtr->m_focused = (focused == GLFW_TRUE);

    // Notify mouse capture system
    if (windowPtr->m_mouseCapture) {
        windowPtr->m_mouseCapture->onWindowFocusChanged(windowPtr->m_focused);
    }

    if (windowPtr->m_focusCallback) {
        windowPtr->m_focusCallback(windowPtr->m_focused);
    }
}

void Window::windowIconifyCallback(GLFWwindow* window, int iconified) {
    auto* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (iconified) {
        windowPtr->m_minimized = true;
    } else {
        windowPtr->m_minimized = false;
    }
}

} // namespace VoxelEngine