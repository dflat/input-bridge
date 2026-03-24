#include <iostream>
#include <string>
#include <vector>
#include <asio.hpp>
#include "network/client.hpp"
#include "network/server.hpp"
#include "input/input.hpp"

void print_usage() {
    std::cout << "Usage:\n"
              << "  input-bridge --server <port>\n"
              << "  input-bridge --client <host> <port>\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
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
                // start_capture is synchronous and blocking or sets up its own callbacks
                // Actually, evdev read is blocking. Best to run it in a separate thread,
                // or integrate its file descriptor into ASIO's posix::stream_descriptor.
                // For now, start_capture will take a pointer to the client to send messages.
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
