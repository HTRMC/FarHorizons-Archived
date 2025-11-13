#include "Window.hpp"
#include <stdexcept>
#include <spdlog/spdlog.h>

namespace FarHorizon {

static bool s_GLFWInitialized = false;

static void glfwErrorCallback(int error, const char* description) {
    spdlog::error("GLFW Error ({}): {}", error, description);
}

Window::Window(const WindowProperties& props)
    : properties_(props)
    , width_(props.width)
    , height_(props.height) {
    init();
}

Window::~Window() {
    shutdown();
}

Window::Window(Window&& other) noexcept
    : window_(other.window_)
    , properties_(std::move(other.properties_))
    , width_(other.width_)
    , height_(other.height_)
    , minimized_(other.minimized_)
    , focused_(other.focused_)
    , resizeCallback_(std::move(other.resizeCallback_))
    , closeCallback_(std::move(other.closeCallback_))
    , focusCallback_(std::move(other.focusCallback_)) {
    other.window_ = nullptr;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        shutdown();

        window_ = other.window_;
        properties_ = std::move(other.properties_);
        width_ = other.width_;
        height_ = other.height_;
        minimized_ = other.minimized_;
        focused_ = other.focused_;
        resizeCallback_ = std::move(other.resizeCallback_);
        closeCallback_ = std::move(other.closeCallback_);
        focusCallback_ = std::move(other.focusCallback_);

        other.window_ = nullptr;
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
    glfwWindowHint(GLFW_RESIZABLE, properties_.resizable ? GLFW_TRUE : GLFW_FALSE);

    // Create window
    GLFWmonitor* monitor = properties_.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    window_ = glfwCreateWindow(
        static_cast<int>(properties_.width),
        static_cast<int>(properties_.height),
        properties_.title.c_str(),
        monitor,
        nullptr
    );

    if (!window_) {
        throw std::runtime_error("Failed to create GLFW window!");
    }

    // Set user pointer for callbacks
    glfwSetWindowUserPointer(window_, this);

    // Setup callbacks
    setupCallbacks();

    // Initialize mouse capture system
    mouseCapture_ = std::make_unique<MouseCapture>(window_);

    spdlog::info("Window created: {}x{}", properties_.width, properties_.height);
}

void Window::shutdown() {
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
}

void Window::setupCallbacks() {
    glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);
    glfwSetWindowCloseCallback(window_, windowCloseCallback);
    glfwSetWindowFocusCallback(window_, windowFocusCallback);
    glfwSetWindowIconifyCallback(window_, windowIconifyCallback);
}

void Window::pollEvents() {
    glfwPollEvents();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window_);
}

void Window::close() {
    glfwSetWindowShouldClose(window_, GLFW_TRUE);
}

void Window::setTitle(const std::string& title) {
    properties_.title = title;
    glfwSetWindowTitle(window_, title.c_str());
}

void Window::setSize(uint32_t width, uint32_t height) {
    glfwSetWindowSize(window_, static_cast<int>(width), static_cast<int>(height));
}

void Window::setVSync(bool enabled) {
    properties_.vsync = enabled;
    // Note: VSync is typically handled by the swap chain in Vulkan
}

void Window::setFullscreen(bool fullscreen) {
    if (properties_.fullscreen == fullscreen) {
        return;
    }

    properties_.fullscreen = fullscreen;

    if (fullscreen) {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(window_, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        glfwSetWindowMonitor(window_, nullptr, 100, 100, properties_.width, properties_.height, 0);
    }
}

void Window::maximize() {
    glfwMaximizeWindow(window_);
}

void Window::minimize() {
    glfwIconifyWindow(window_);
}

void Window::restore() {
    glfwRestoreWindow(window_);
}

void Window::setCursorMode(int mode) {
    glfwSetInputMode(window_, GLFW_CURSOR, mode);
}

void Window::setCursorPos(double x, double y) {
    glfwSetCursorPos(window_, x, y);
}

// Static callbacks
void Window::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));
    windowPtr->width_ = static_cast<uint32_t>(width);
    windowPtr->height_ = static_cast<uint32_t>(height);
    windowPtr->minimized_ = (width == 0 || height == 0);

    // Notify mouse capture system
    if (windowPtr->mouseCapture_) {
        windowPtr->mouseCapture_->onWindowResized(width, height);
    }

    if (windowPtr->resizeCallback_ && !windowPtr->minimized_) {
        windowPtr->resizeCallback_(windowPtr->width_, windowPtr->height_);
    }
}

void Window::windowCloseCallback(GLFWwindow* window) {
    auto* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (windowPtr->closeCallback_) {
        windowPtr->closeCallback_();
    }
}

void Window::windowFocusCallback(GLFWwindow* window, int focused) {
    auto* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));
    windowPtr->focused_ = (focused == GLFW_TRUE);

    // Notify mouse capture system
    if (windowPtr->mouseCapture_) {
        windowPtr->mouseCapture_->onWindowFocusChanged(windowPtr->focused_);
    }

    if (windowPtr->focusCallback_) {
        windowPtr->focusCallback_(windowPtr->focused_);
    }
}

void Window::windowIconifyCallback(GLFWwindow* window, int iconified) {
    auto* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (iconified) {
        windowPtr->minimized_ = true;
    } else {
        windowPtr->minimized_ = false;
    }
}

} // namespace FarHorizon