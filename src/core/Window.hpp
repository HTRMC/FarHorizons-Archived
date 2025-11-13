#pragma once

#include <GLFW/glfw3.h>
#include <string>
#include <functional>
#include <cstdint>
#include <memory>
#include "MouseCapture.hpp"

namespace FarHorizon {

struct WindowProperties {
    std::string title = "Vulkan Voxel Engine";
    uint32_t width = 1280;
    uint32_t height = 720;
    bool vsync = true;
    bool resizable = true;
    bool fullscreen = false;
};

class Window {
public:
    using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;
    using CloseCallback = std::function<void()>;
    using FocusCallback = std::function<void(bool focused)>;

    explicit Window(const WindowProperties& props = WindowProperties{});
    ~Window();

    // Prevent copying
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Allow moving
    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    // Core functionality
    void pollEvents();
    bool shouldClose() const;
    void close();

    // Window state
    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }
    float getAspectRatio() const { return static_cast<float>(width_) / static_cast<float>(height_); }
    bool isMinimized() const { return minimized_; }
    bool isFocused() const { return focused_; }

    // Window manipulation
    void setTitle(const std::string& title);
    void setSize(uint32_t width, uint32_t height);
    void setVSync(bool enabled);
    void setFullscreen(bool fullscreen);
    void maximize();
    void minimize();
    void restore();

    // Cursor control (legacy - use MouseCapture instead)
    void setCursorMode(int mode); // GLFW_CURSOR_NORMAL, GLFW_CURSOR_HIDDEN, GLFW_CURSOR_DISABLED
    void setCursorPos(double x, double y);

    // Mouse capture system
    MouseCapture* getMouseCapture() const { return mouseCapture_.get(); }

    // Callbacks
    void setResizeCallback(ResizeCallback callback) { resizeCallback_ = callback; }
    void setCloseCallback(CloseCallback callback) { closeCallback_ = callback; }
    void setFocusCallback(FocusCallback callback) { focusCallback_ = callback; }

    // GLFW access
    GLFWwindow* getNativeWindow() const { return window_; }

private:
    void init();
    void shutdown();
    void setupCallbacks();

    // GLFW callbacks
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void windowCloseCallback(GLFWwindow* window);
    static void windowFocusCallback(GLFWwindow* window, int focused);
    static void windowIconifyCallback(GLFWwindow* window, int iconified);

private:
    GLFWwindow* window_ = nullptr;
    WindowProperties properties_;

    uint32_t width_;
    uint32_t height_;
    bool minimized_ = false;
    bool focused_ = true;

    // User callbacks
    ResizeCallback resizeCallback_;
    CloseCallback closeCallback_;
    FocusCallback focusCallback_;

    // Mouse capture system
    std::unique_ptr<MouseCapture> mouseCapture_;
};

} // namespace FarHorizon