#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>

#include <memory>

namespace mini_ipc {

class BridgeDispatcher;

class WebSocketServer {
public:
    WebSocketServer(const WebSocketServer& other) = delete;
    WebSocketServer& operator=(const WebSocketServer& other) = delete;

    WebSocketServer(boost::asio::io_context& ioc,
                    boost::asio::ip::tcp::endpoint endpoint,
                    BridgeDispatcher& disaptcher);

    void run();

private:
    void do_accept();
    void on_accept(boost::system::error_code ec,
                   boost::asio::ip::tcp::socket socket);

    boost::asio::io_context& ioc_;
    boost::asio::ip::tcp::acceptor acceptor_;
    BridgeDispatcher& disaptcher_;
};
} // namespace mini_ipc
