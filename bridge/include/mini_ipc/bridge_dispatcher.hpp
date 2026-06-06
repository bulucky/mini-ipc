/**
 *@brief: 任务调度, 内部持有一个node节点示例, 以topic为核心创建对应的发布者或订阅者实现core与dashboard的数据转发
 */

#pragma once

#include "mini_ipc/node.hpp"
#include "mini_ipc/publisher.hpp"
#include "mini_ipc/subscriber.hpp"

#include <boost/asio/io_context.hpp>

#include <string>
#include <unordered_map>

namespace mini_ipc {
class WebSocketSession;

class BridgeDispatcher {
public:
    /**
     *@brief: 构造函数
     */
    BridgeDispatcher(Node& node, boost::asio::io_context& ioc);

    /**
     *@brief: 处理请求消息
     */
    std::string handle_message(const std::string& text, WebSocketSession* session, std::weak_ptr<WebSocketSession> session_weak_ptr);

private:
    /**
     *@brief: 处理发布
     * dashboard -->bridge --> core
     */
    std::string handle_publish(const std::string& topic,
                               const std::string& payload);

    /**
     *@brief: 处理订阅
     * core --> bridge --> dashboard
     *@param: const std::string& topic
     */
    std::string handle_subscribe(const std::string& topic, WebSocketSession* session, std::weak_ptr<WebSocketSession> session_weak_ptr);

    /**
     *@brief: 缓存发布者
     *
     *@param: const std::string& topic
     */
    Publisher& get_or_create_publisher(const std::string& topic);

    /**
     *@brief: 封装status消息字符串
     */
    static std::string make_status(const std::string& level,
                                   const std::string& message);

    /**
     *@brief: 封装message消息字符串
     */
    static std::string make_message(const std::string& topic,
                                    const std::string& payload);

    Node& node_;
    boost::asio::io_context& ioc_;
    std::unordered_map<std::string, Publisher> publishers_;
    std::unordered_map<WebSocketSession*, Subscriber> subscribers_;
};
} // namespace mini_ipc