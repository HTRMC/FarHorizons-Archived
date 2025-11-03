#pragma once

#include <GLFW/glfw3.h>
#include <string>
#include <functional>
#include <cstdint>
#include <memory>

namespace VoxelEngine {

class MouseCapture;

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
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }
    float getAspectRatio() const { return static_cast<float>(m_width) / static_cast<float>(m_height); }
    bool isMinimized() const { return m_minimized; }
    bool isFocused() const { return m_focused; }

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
    MouseCapture* getMouseCapture() const { return m_mouseCapture.get(); }

    // Callbacks
    void setResizeCallback(ResizeCallback callback) { m_resizeCallback = callback; }
    void setCloseCallback(CloseCallback callback) { m_closeCallback = callback; }
    void setFocusCallback(FocusCallback callback) { m_focusCallback = callback; }

    // GLFW access
    GLFWwindow* getNativeWindow() const { return m_window; }

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
    GLFWwindow* m_window = nullptr;
    WindowProperties m_properties;

    uint32_t m_width;
    uint32_t m_height;
    bool m_minimized = false;
    bool m_focused = true;

    // User callbacks
    ResizeCallback m_resizeCallback;
    CloseCallback m_closeCallback;
    FocusCallback m_focusCallback;

    // Mouse capture system
    std::unique_ptr<MouseCapture> m_mouseCapture;
};

} // namespace VoxelEngine