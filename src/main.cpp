#include <iostream>
#include "core/Window.hpp"
#include "core/Input.hpp"
#include "events/Event.hpp"
#include "events/EventBus.hpp"

using namespace VoxelEngine;

int main() {
    try {
        // Create window with custom properties
        WindowProperties props;
        props.title = "Vulkan Voxel Engine - Input Demo";
        props.width = 1600;
        props.height = 900;
        props.vsync = true;
        props.resizable = true;

        Window window(props);

        // Initialize input system
        Input::init(window.getNativeWindow());

        // Subscribe to window events
        auto resizeHandle = EventBus::subscribe<WindowResizeEvent>([](WindowResizeEvent& e) {
            std::cout << "[Event] " << e.toString() << std::endl;
        });

        auto closeHandle = EventBus::subscribe<WindowCloseEvent>([](WindowCloseEvent& e) {
            std::cout << "[Event] Window closing..." << std::endl;
        });

        auto focusHandle = EventBus::subscribe<WindowFocusEvent>([](WindowFocusEvent& e) {
            std::cout << "[Event] Window " << (e.isFocused() ? "focused" : "unfocused") << std::endl;
        });

        // Set up window callbacks to post events
        window.setResizeCallback([](uint32_t width, uint32_t height) {
            WindowResizeEvent event(width, height);
            EventBus::post(event);
        });

        window.setCloseCallback([]() {
            WindowCloseEvent event;
            EventBus::post(event);
        });

        window.setFocusCallback([](bool focused) {
            WindowFocusEvent event(focused);
            EventBus::post(event);
        });

        std::cout << "=== Vulkan Voxel Engine - Input System Demo ===" << std::endl;
        std::cout << "Controls:" << std::endl;
        std::cout << "  WASD - Move" << std::endl;
        std::cout << "  Space - Jump" << std::endl;
        std::cout << "  Shift - Sprint" << std::endl;
        std::cout << "  Ctrl - Crouch" << std::endl;
        std::cout << "  Mouse - Look around" << std::endl;
        std::cout << "  Mouse Wheel - Zoom" << std::endl;
        std::cout << "  Left Click - Primary action" << std::endl;
        std::cout << "  Right Click - Secondary action" << std::endl;
        std::cout << "  F - Toggle fullscreen" << std::endl;
        std::cout << "  L - Lock cursor" << std::endl;
        std::cout << "  ESC - Exit" << std::endl;
        std::cout << "===============================================" << std::endl;

        bool cursorLocked = false;
        glm::vec2 lastMousePos = Input::getMousePosition();

        // Main loop
        while (!window.shouldClose()) {
            // Poll window events
            window.pollEvents();

            // Update input state
            Input::update();

            // Process queued events
            EventBus::processQueue();

            // === Keyboard Input Demo ===

            // Movement
            if (Input::isKeyPressed(KeyCode::W)) {
                // Move forward
            }
            if (Input::isKeyPressed(KeyCode::S)) {
                // Move backward
            }
            if (Input::isKeyPressed(KeyCode::A)) {
                // Move left
            }
            if (Input::isKeyPressed(KeyCode::D)) {
                // Move right
            }

            // Jump (only when first pressed, not held)
            if (Input::isKeyDown(KeyCode::Space)) {
                std::cout << "[Input] Jump!" << std::endl;
            }

            // Sprint modifier
            if (Input::isShiftPressed()) {
                // Apply sprint multiplier
            }

            // Crouch modifier
            if (Input::isControlPressed()) {
                // Apply crouch
            }

            // Toggle fullscreen
            if (Input::isKeyDown(KeyCode::F)) {
                static bool isFullscreen = false;
                isFullscreen = !isFullscreen;
                window.setFullscreen(isFullscreen);
                std::cout << "[Input] Fullscreen: " << (isFullscreen ? "ON" : "OFF") << std::endl;
            }

            // Toggle cursor lock
            if (Input::isKeyDown(KeyCode::L)) {
                cursorLocked = !cursorLocked;
                window.setCursorMode(cursorLocked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
                std::cout << "[Input] Cursor: " << (cursorLocked ? "LOCKED" : "UNLOCKED") << std::endl;
            }

            // Exit on ESC
            if (Input::isKeyDown(KeyCode::Escape)) {
                std::cout << "[Input] ESC pressed - closing window" << std::endl;
                window.close();
            }

            // === Mouse Input Demo ===

            // Mouse buttons
            if (Input::isMouseButtonDown(MouseButton::Left)) {
                glm::vec2 pos = Input::getMousePosition();
                std::cout << "[Input] Left click at (" << pos.x << ", " << pos.y << ")" << std::endl;
            }

            if (Input::isMouseButtonDown(MouseButton::Right)) {
                glm::vec2 pos = Input::getMousePosition();
                std::cout << "[Input] Right click at (" << pos.x << ", " << pos.y << ")" << std::endl;
            }

            // Mouse movement (for camera control)
            glm::vec2 mouseDelta = Input::getMouseDelta();
            if (cursorLocked && glm::length(mouseDelta) > 0.1f) {
                // Apply camera rotation based on mouse delta
                // Example: camera.rotate(mouseDelta.x * sensitivity, mouseDelta.y * sensitivity);
            }

            // Mouse scroll
            glm::vec2 scroll = Input::getMouseScroll();
            if (scroll.y != 0.0f) {
                std::cout << "[Input] Mouse scroll: " << scroll.y << std::endl;
            }

            // === Gamepad Input Demo ===

            if (Input::isGamepadConnected()) {
                // Gamepad buttons
                if (Input::isGamepadButtonDown(GamepadButton::A)) {
                    std::cout << "[Input] Gamepad A button pressed" << std::endl;
                }

                // Analog sticks
                glm::vec2 leftStick = Input::getGamepadLeftStick();
                glm::vec2 rightStick = Input::getGamepadRightStick();

                // Apply deadzone
                const float deadzone = 0.15f;
                if (glm::length(leftStick) > deadzone) {
                    // Move character based on left stick
                }
                if (glm::length(rightStick) > deadzone) {
                    // Rotate camera based on right stick
                }

                // Triggers
                float leftTrigger = Input::getGamepadAxis(GamepadAxis::LeftTrigger);
                float rightTrigger = Input::getGamepadAxis(GamepadAxis::RightTrigger);
            }

            // === Render Loop Would Go Here ===
            // In a real implementation, this is where you'd:
            // 1. Update game logic
            // 2. Render frame with Vulkan
            // 3. Present to screen
        }

        // Clean up
        EventBus::unsubscribe(resizeHandle);
        EventBus::unsubscribe(closeHandle);
        EventBus::unsubscribe(focusHandle);
        EventBus::clear();

        std::cout << "Application shutting down..." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}