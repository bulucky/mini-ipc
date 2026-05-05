#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket/stream.hpp>

namespace mini_ipc {
class BridgeDispatcher;

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    /**
     *@brief: 构造函数，将tcp socket包装为websocket stream
     */
    WebSocketSession(boost::asio::ip::tcp::socket socket,
                     BridgeDispatcher& dispather);

    /**
     *@brief: 等待客户端发起握手
     */
    void run();

private:
    /**
     *@brief: 接受握手回调
     */
    void on_accept(boost::system::error_code ec);

    /**
     *@brief: 发起异步读取
     */
    void do_read();

    /**
     *@brief: 读取回调
     */
    void on_read(boost::system::error_code ec,
                 std::size_t bytes_transferred);

    /**
     *@brief: 发起异步写入
     */
    void do_write(std::string text);

    /**
     *@brief: 写入回调
     */
    void on_write(boost::system::error_code ec,
                  std::size_t bytes_transferred);


    boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
    boost::beast::flat_buffer read_buffer_;
    BridgeDispatcher& dispatcher_;
    std::string write_buffer_;
};
} // namespace mini_ipc