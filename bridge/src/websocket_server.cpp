#include "mini_ipc/websocket_server.hpp"
#include "mini_ipc/websocket_session.hpp"

#include <boost/beast/core/error.hpp>
#include <boost/system/error_code.hpp>

#include <iostream>


namespace mini_ipc {
using tcp = boost::asio::ip::tcp;

WebSocketServer::WebSocketServer(boost::asio::io_context& ioc,
                                 boost::asio::ip::tcp::endpoint endpoint, // NOLINT
                                 BridgeDispatcher& disaptcher)
    : ioc_(ioc), acceptor_(ioc), disaptcher_(disaptcher) {
    boost::system::error_code ec;
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
        std::cerr << "[WebSocketServer] open failed: " << ec.message() << "\n";
        return;
    }

    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec) {
        std::cerr << "[WebSocketServer] reuse_address failed: " << ec.message() << "\n";
        return;
    }

    acceptor_.bind(endpoint, ec);
    if (ec) {
        std::cerr << "[WebSocketServer] bind failed: " << ec.message() << "\n";
        return;
    }

    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
        std::cerr << "[WebSocketServer] listen failed: " << ec.message() << "\n";
        return;
    }

    ready_ = true;

    std::cout << "[WebSocketServer] listening on "
              << endpoint.address().to_string()
              << ":"
              << endpoint.port()
              << "\n";
}
void WebSocketServer::run() {
    if (!ready_) {
        std::cerr << "[WebSocketServer] server is not ready, skip accept\n";
        return;
    }

    do_accept();
}

void WebSocketServer::do_accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            on_accept(ec, std::move(socket));
        });
}

void WebSocketServer::on_accept(boost::system::error_code ec,
                                boost::asio::ip::tcp::socket socket) {
    if (ec) {
        std::cerr << "[WebSocketServer] accept failed: " << ec.message() << "\n";
        if (!acceptor_.is_open()) {
            return;
        }
    } else {
        std::cout << "[WebSocketServer] new tcp connection\n";

        std::make_shared<WebSocketSession>(
            std::move(socket), disaptcher_)
            ->run();
    }

    do_accept();
}
}; // namespace mini_ipc
