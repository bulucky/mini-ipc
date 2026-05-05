#include "mini_ipc/websocket_session.hpp"
#include "mini_ipc/bridge_dispatcher.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/buffers_to_string.hpp>

#include <iostream>


namespace mini_ipc {
namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;
using tcp = boost::asio::ip::tcp;

WebSocketSession::WebSocketSession(boost::asio::ip::tcp::socket socket,
                                   BridgeDispatcher& dispather)
    : ws_(std::move(socket)), dispatcher_(dispather) {}

void WebSocketSession::run() {
    ws_.async_accept(
        beast::bind_front_handler(
            &WebSocketSession::on_accept,
            shared_from_this()));
}

void WebSocketSession::on_accept(boost::system::error_code ec) {
    if (ec) {
        std::cerr << "[WebSocketSession] websocket accept failed: "
                  << ec.message() << "\n";
        return;
    }

    std::cout << "[WebSocketSession] websocket connected\n";
    do_read();
}

void WebSocketSession::do_read() {
    ws_.async_read(
        read_buffer_,
        beast::bind_front_handler(
            &WebSocketSession::on_read,
            shared_from_this()));
}

void WebSocketSession::on_read(boost::system::error_code ec,
                               std::size_t bytes_transferred) {
    (void)bytes_transferred;

    if (ec == websocket::error::closed) {
        std::cerr << "[WebSocketSession] read failed: "
                  << ec.message() << "\n";
        return;
    }

    std::string request_msg = beast::buffers_to_string(read_buffer_.data());

    read_buffer_.consume(read_buffer_.size());

    std::string respone_msg = dispatcher_.handle_message(request_msg);

    do_write(respone_msg);
}

void WebSocketSession::do_write(std::string text) {
    write_buffer_ = std::move(text);

    // 文本帧, json
    ws_.text(true);

    ws_.async_write(
        boost::asio::buffer(write_buffer_),
        beast::bind_front_handler(
            &WebSocketSession::on_write,
            shared_from_this()));
}

void WebSocketSession::on_write(boost::system::error_code ec,
                                std::size_t bytes_transferred) {
    (void)bytes_transferred;
    if (ec) {
        std::cerr << "[WebSocketSession] write failed: "
                  << ec.message() << "\n";
        return;
    }

    write_buffer_.clear();

    do_read();
}

} // namespace mini_ipc