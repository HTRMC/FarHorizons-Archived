#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "client/FarHorizonClient.hpp"

using namespace FarHorizon;

/**
 * Far Horizon - Main entry point
 */
int main() {
    try {
        // Initialize logging
        spdlog::init_thread_pool(8192, 1);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto logger = std::make_shared<spdlog::async_logger>("main", console_sink,
                                                             spdlog::thread_pool(),
                                                             spdlog::async_overflow_policy::block);
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::debug);
        spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

        spdlog::info("=== Far Horizon ===");
        spdlog::info("Initializing...");

        // Create and run the client
        FarHorizonClient client;
        client.init();
        client.run();
        client.shutdown();

        spdlog::info("Application shutting down...");
        spdlog::shutdown();

    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        spdlog::shutdown();
        return 1;
    }

    return 0;
}
