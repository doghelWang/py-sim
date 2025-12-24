#include "amr/AppModel.hpp"
#include "amr/WebServer.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <pybind11/embed.h>

int main() {
    std::cout << "Starting PySim Server..." << std::endl;

    // 0. Init Python Interpreter (Global)
    // Prevents crash when threads exit or modules unload
    pybind11::scoped_interpreter guard{};

    // 1. Init Core
    auto& model = amr::AppModel::Instance();
    // model.Initialize(); // Constructor handles init

    // 2. Start Web Server
    amr::WebServer server(model);
    server.Start(8080);

    // 3. Main Loop (Physics & Logic)
    // Run at ~100Hz
    using clock = std::chrono::high_resolution_clock;
    auto next_frame = clock::now();

    while (true) {
        // Physics Step
        model.UpdatePhysics(0.01f); // Updates Physics and executes Python steps if active

        // Timing
        next_frame += std::chrono::milliseconds(10);
        std::this_thread::sleep_until(next_frame);
    }

    return 0;
}
