#pragma once

#include <asio.hpp>
#include <memory>
#include <functional>
#include "../protocol/input_event.hpp"

namespace network {

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(asio::ip::tcp::socket socket, std::function<void(const protocol::InputEvent&)> on_event);

    void start();

private:
    void do_read();

    asio::ip::tcp::socket socket_;
    protocol::InputEvent read_msg_;
    std::function<void(const protocol::InputEvent&)> on_event_;
};

class Server {
public:
    Server(asio::io_context& io_context, short port);
    
    // Set callback for when an event is received
    void set_on_event(std::function<void(const protocol::InputEvent&)> callback);

private:
    void do_accept();

    asio::ip::tcp::acceptor acceptor_;
    std::function<void(const protocol::InputEvent&)> on_event_;
};

} // namespace network