#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <queue>

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

    /**
     *@brief: 外部向该会话推送消息, 由 BridgeDispatcher 的 subscriber 回调触发
     */
    void send_message(std::string text);

private:
    /**
     *@brief: 接受握手回调函数
     */
    void on_accept(boost::system::error_code ec);

    /**
     *@brief: 发起异步读取
     */
    void do_read();

    /**
     *@brief: 读取回调函数
     */
    void on_read(boost::system::error_code ec,
                 std::size_t bytes_transferred);

    /**
     *@brief: 发起异步写入
     */
    void do_write();

    /**
     *@brief: 写入回调函数
     */
    void on_write(boost::system::error_code ec,
                  std::size_t bytes_transferred);


    boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
    boost::beast::flat_buffer read_buffer_;
    BridgeDispatcher& dispatcher_;
    std::string write_buffer_;

    std::queue<std::string> write_queue_;
    bool writing_ = false;
};
} // namespace mini_ipc