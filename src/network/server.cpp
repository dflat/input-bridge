#include "server.hpp"
#include <iostream>

namespace network {

Session::Session(asio::ip::tcp::socket socket, std::function<void(const protocol::InputEvent&)> on_event)
    : socket_(std::move(socket)), on_event_(std::move(on_event)) {
}

void Session::start() {
    // Disable Nagle's algorithm for low latency
    asio::ip::tcp::no_delay option(true);
    socket_.set_option(option);
    do_read();
}

void Session::do_read() {
    auto self(shared_from_this());
    asio::async_read(socket_,
        asio::buffer(&read_msg_, sizeof(protocol::InputEvent)),
        [this, self](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                if (on_event_) {
                    on_event_(read_msg_);
                }
                do_read();
            } else {
                std::cerr << "Session closed: " << ec.message() << std::endl;
            }
        });
}

Server::Server(asio::io_context& io_context, short port)
    : acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {
}

void Server::set_on_event(std::function<void(const protocol::InputEvent&)> callback) {
    on_event_ = callback;
    do_accept(); // Start accepting only when callback is set
}

void Server::do_accept() {
    acceptor_.async_accept(
        [this](std::error_code ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                std::cout << "Accepted new connection." << std::endl;
                std::make_shared<Session>(std::move(socket), on_event_)->start();
            } else {
                std::cerr << "Accept error: " << ec.message() << std::endl;
            }

            // Accept the next connection
            do_accept();
        });
}

} // namespace network