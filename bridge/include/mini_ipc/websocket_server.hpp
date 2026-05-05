#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>

namespace mini_ipc {

class BridgeDispatcher;

class WebSocketServer {
public:
    WebSocketServer(const WebSocketServer& other) = delete;
    WebSocketServer& operator=(const WebSocketServer& other) = delete;

    /**
     *@brief: 构造函数，完成tcp server初始化
     */
    WebSocketServer(boost::asio::io_context& ioc,
                    boost::asio::ip::tcp::endpoint endpoint,
                    BridgeDispatcher& disaptcher);


    /**
     *@brief: 启动server后等待客户端链接
     */
    void run();

private:
    /**
     *@brief: 异步接受链接
     */
    void do_accept();

    /**
     *@brief: 异步处理链接
     *  成功则新建会话管理链接并等待下一个链接，失败则直接返回
     *
     */
    void on_accept(boost::system::error_code ec,
                   boost::asio::ip::tcp::socket socket);


    boost::asio::io_context& ioc_;
    boost::asio::ip::tcp::acceptor acceptor_;
    BridgeDispatcher& disaptcher_;
};
} // namespace mini_ipc
