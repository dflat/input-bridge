#include "client.hpp"
#include <iostream>
#include <deque>
#include <functional>

namespace network {

Client::Client(asio::io_context& io_context, const std::string& host, const std::string& port)
    : io_context_(io_context),
      socket_(io_context),
      host_(host),
      port_(port) {
}

void Client::set_on_connected(std::function<void()> callback) {
    on_connected_ = callback;
}

void Client::set_on_disconnected(std::function<void()> callback) {
    on_disconnected_ = callback;
}

void Client::start() {
    asio::ip::tcp::resolver resolver(io_context_);
    auto endpoints = resolver.resolve(host_, port_);
    do_connect(endpoints);
}

void Client::send_event(const protocol::InputEvent& event) {
    asio::post(io_context_,
        [this, event]() {
            bool write_in_progress = !write_msgs_.empty();
            write_msgs_.push_back(event);
            if (!write_in_progress) {
                do_write();
            }
        });
}

void Client::do_connect(const asio::ip::tcp::resolver::results_type& endpoints) {
    asio::async_connect(socket_, endpoints,
        [this](std::error_code ec, asio::ip::tcp::endpoint) {
            if (!ec) {
                std::cout << "Connected to server." << std::endl;
                
                // Disable Nagle's algorithm for low latency input forwarding
                asio::ip::tcp::no_delay option(true);
                socket_.set_option(option);

                if (on_connected_) {
                    on_connected_();
                }
                
                // Start a read operation to keep ASIO event loop alive and detect disconnections
                do_read();
            } else {
                std::cerr << "Connect failed: " << ec.message() << ". Retrying in 1s..." << std::endl;
                if (on_disconnected_) {
                    on_disconnected_();
                }
                
                // Simple retry logic
                auto timer = std::make_shared<asio::steady_timer>(io_context_, asio::chrono::seconds(1));
                timer->async_wait([this, timer](std::error_code /*ec*/) {
                    start();
                });
            }
        });
}

void Client::do_write() {
    if (write_msgs_.empty()) return;
    
    asio::async_write(socket_,
        asio::buffer(&write_msgs_.front(), sizeof(protocol::InputEvent)),
        [this](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                write_msgs_.pop_front();
                if (!write_msgs_.empty()) {
                    do_write();
                }
            } else {
                std::cerr << "Write failed: " << ec.message() << std::endl;
                socket_.close();
                if (on_disconnected_) {
                    on_disconnected_();
                }
                // Try to reconnect
                start();
            }
        });
}

void Client::do_read() {
    asio::async_read(socket_,
        asio::buffer(read_buffer_, 1),
        [this](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                // Keep reading if server somehow sends data
                do_read();
            } else {
                // Server disconnected or error occurred
                std::cerr << "Disconnected from server: " << ec.message() << std::endl;
                socket_.close();
                if (on_disconnected_) {
                    on_disconnected_();
                }
                // Try to reconnect
                start();
            }
        });
}

} // namespace network