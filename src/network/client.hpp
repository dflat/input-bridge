#pragma once

#include <string>
#include <deque>
#include <asio.hpp>
#include "../protocol/input_event.hpp"

namespace network {

class Client {
public:
    Client(asio::io_context& io_context, const std::string& host, const std::string& port);
    
    // Starts the connection attempt
    void start();
    
    // Send an input event asynchronously
    void send_event(const protocol::InputEvent& event);

    // Callbacks/Hooks
    void set_on_connected(std::function<void()> callback);
    void set_on_disconnected(std::function<void()> callback);

private:
    void do_connect(const asio::ip::tcp::resolver::results_type& endpoints);
    void do_write();
    void do_read();

    asio::io_context& io_context_;
    asio::ip::tcp::socket socket_;
    std::string host_;
    std::string port_;
    
    std::deque<protocol::InputEvent> write_msgs_;
    char read_buffer_[1]; // Just for detecting disconnects
    std::function<void()> on_connected_;
    std::function<void()> on_disconnected_;
};

} // namespace network