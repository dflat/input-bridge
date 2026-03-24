#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <asio.hpp>
#include "network/client.hpp"
#include "network/server.hpp"
#include "input/input.hpp"

void print_usage() {
    std::cout << "Usage:\n"
              << "  input-bridge --server <port>\n"
              << "  input-bridge --client <host> <port>\n"
              << "  input-bridge --dry-run\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string mode = argv[1];
    
    try {
        asio::io_context io_context;

        if (mode == "--server") {
#ifdef __APPLE__
            if (argc != 3) {
                print_usage();
                return 1;
            }
            short port = std::stoi(argv[2]);
            
            input::init_injection();

            network::Server server(io_context, port);
            server.set_on_event([](const protocol::InputEvent& event) {
                input::handle_event(event);
            });

            std::cout << "Server listening on port " << port << "..." << std::endl;
            io_context.run();
#else
            std::cerr << "Error: Server mode is only supported on macOS." << std::endl;
            return 1;
#endif
        } else if (mode == "--client") {
#ifdef __linux__
            if (argc != 4) {
                print_usage();
                return 1;
            }
            std::string host = argv[2];
            std::string port = argv[3];

            network::Client client(io_context, host, port);
            
            client.set_on_connected([&]() {
                input::start_capture(reinterpret_cast<void*>(&client));
            });

            client.set_on_disconnected([&]() {
                input::stop_capture();
            });

            std::cout << "Client attempting to connect to " << host << ":" << port << "..." << std::endl;
            client.start();
            io_context.run();
#else
            std::cerr << "Error: Client mode is only supported on Linux." << std::endl;
            return 1;
#endif
        } else if (mode == "--dry-run") {
#ifdef __linux__
            std::cout << "Starting dry-run mode. Inputs will be captured and printed locally." << std::endl;
            std::cout << "Press Left Alt + Backslash to exit." << std::endl;
            
            input::start_capture(nullptr);

            // Keep main thread alive while background capture thread runs
            while(true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
#else
            std::cerr << "Error: Dry-run mode is only supported on Linux." << std::endl;
            return 1;
#endif
        } else {
            std::cerr << "Unknown mode: " << mode << std::endl;
            print_usage();
            return 1;
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
