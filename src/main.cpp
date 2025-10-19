#include <iostream>
#include "core/Window.hpp"
#include "core/InputSystem.hpp"
#include "core/InputAction.hpp"
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

        // Initialize AAA input system with event queue
        InputSystem::init(window.getNativeWindow());

        // Set up Input Actions (Unreal-style)
        auto& jumpAction = InputActionManager::createAction("Jump");
        jumpAction.addBinding(InputBinding(KeyCode::Space));
        jumpAction.addBinding(InputBinding(GamepadButton::A));
        jumpAction.bind([]() {
            std::cout << "[Action] Jump!" << std::endl;
        });

        auto& fireAction = InputActionManager::createAction("Fire");
        fireAction.addBinding(InputBinding(MouseButton::Left));
        fireAction.addBinding(InputBinding(GamepadButton::RightBumper));
        fireAction.bind([]() {
            std::cout << "[Action] Fire!" << std::endl;
        });

        // Set up Input Axes (continuous values)
        auto& moveForwardAxis = InputActionManager::createAxis("MoveForward");
        moveForwardAxis.addBinding(InputBinding(KeyCode::W, 1.0f));
        moveForwardAxis.addBinding(InputBinding(KeyCode::S, -1.0f));
        moveForwardAxis.addBinding(InputBinding(GamepadAxis::LeftY, -1.0f)); // Inverted Y
        moveForwardAxis.bind([](float value) {
            // std::cout << "[Axis] MoveForward: " << value << std::endl;
        });

        auto& moveRightAxis = InputActionManager::createAxis("MoveRight");
        moveRightAxis.addBinding(InputBinding(KeyCode::D, 1.0f));
        moveRightAxis.addBinding(InputBinding(KeyCode::A, -1.0f));
        moveRightAxis.addBinding(InputBinding(GamepadAxis::LeftX, 1.0f));
        moveRightAxis.bind([](float value) {
            // std::cout << "[Axis] MoveRight: " << value << std::endl;
        });

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

        std::cout << "=== Vulkan Voxel Engine - AAA Input System Demo ===" << std::endl;
        std::cout << "Features:" << std::endl;
        std::cout << "  - Event Queue (zero input loss)" << std::endl;
        std::cout << "  - Thread-Safe callbacks" << std::endl;
        std::cout << "  - Timestamped events" << std::endl;
        std::cout << "  - Input Action Mapping (Unreal-style)" << std::endl;
        std::cout << "  - Professional deadzone handling" << std::endl;
        std::cout << "\nControls:" << std::endl;
        std::cout << "  WASD / Left Stick - Move" << std::endl;
        std::cout << "  Space / A Button - Jump" << std::endl;
        std::cout << "  Shift - Sprint" << std::endl;
        std::cout << "  Ctrl - Crouch" << std::endl;
        std::cout << "  Mouse / Right Stick - Look" << std::endl;
        std::cout << "  Mouse Wheel - Zoom" << std::endl;
        std::cout << "  Left Click / RT - Fire" << std::endl;
        std::cout << "  F - Toggle fullscreen" << std::endl;
        std::cout << "  L - Lock cursor" << std::endl;
        std::cout << "  ESC - Exit" << std::endl;
        std::cout << "=====================================================" << std::endl;

        bool cursorLocked = false;

        // Main loop
        while (!window.shouldClose()) {
            // Poll window events (GLFW)
            window.pollEvents();

            // Process input events from queue (AAA system!)
            InputSystem::processEvents();

            // Process input actions (Unreal-style)
            InputActionManager::processInput();

            // Process window events
            EventBus::processQueue();

            // === Direct Input Queries (still available!) ===

            // Sprint modifier
            if (InputSystem::isShiftPressed()) {
                // Apply sprint multiplier
            }

            // Crouch modifier
            if (InputSystem::isControlPressed()) {
                // Apply crouch
            }

            // Toggle fullscreen
            if (InputSystem::isKeyDown(KeyCode::F)) {
                static bool isFullscreen = false;
                isFullscreen = !isFullscreen;
                window.setFullscreen(isFullscreen);
                std::cout << "[Input] Fullscreen: " << (isFullscreen ? "ON" : "OFF") << std::endl;
            }

            // Toggle cursor lock
            if (InputSystem::isKeyDown(KeyCode::L)) {
                cursorLocked = !cursorLocked;
                window.setCursorMode(cursorLocked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
                std::cout << "[Input] Cursor: " << (cursorLocked ? "LOCKED" : "UNLOCKED") << std::endl;
            }

            // Exit on ESC
            if (InputSystem::isKeyDown(KeyCode::Escape)) {
                std::cout << "[Input] ESC pressed - closing window" << std::endl;
                window.close();
            }

            // === Mouse Demo ===
            glm::vec2 mouseDelta = InputSystem::getMouseDelta();
            if (cursorLocked && glm::length(mouseDelta) > 0.1f) {
                // Apply camera rotation
                // camera.rotate(mouseDelta.x * sensitivity, mouseDelta.y * sensitivity);
            }

            glm::vec2 scroll = InputSystem::getMouseScroll();
            if (scroll.y != 0.0f) {
                std::cout << "[Input] Mouse scroll: " << scroll.y << std::endl;
            }

            // === Gamepad Demo ===
            if (InputSystem::isGamepadConnected()) {
                glm::vec2 rightStick = InputSystem::getGamepadRightStick();
                if (glm::length(rightStick) > 0.01f) {
                    // Rotate camera with right stick (deadzone already applied!)
                    // camera.rotate(rightStick.x * sensitivity, rightStick.y * sensitivity);
                }
            }

            // === Render Loop Would Go Here ===
            // In a real implementation, this is where you'd:
            // 1. Update game logic
            // 2. Render frame with Vulkan
            // 3. Present to screen
        }

        // Clean up
        InputSystem::shutdown();
        InputActionManager::clear();
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