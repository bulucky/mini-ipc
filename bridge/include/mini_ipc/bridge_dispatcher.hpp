/**
 *@brief: WebSocket JSON 协议和 MiniIPC Core API 之间的适配层
 */

#pragma once

#include "mini_ipc/node.hpp"
#include "mini_ipc/publisher.hpp"
#include "mini_ipc/subscriber.hpp"

#include <string>
#include <unordered_map>

namespace mini_ipc {

class BridgeDispatcher {
public:
    /**
     *@brief: 构造函数
     */
    explicit BridgeDispatcher(Node& node);

    /**
     *@brief: 处理请求消息
     */
    std::string handle_message(const std::string& text);

private:
    /**
     *@brief: 处理发布
     * dashboard --> core
     */
    std::string handle_publish(const std::string& topic,
                               const std::string& payload);

    /**
     *@brief: 处理订阅
     * core --> dashboard
     *@param: const std::string& topic
     */
    std::string handle_subscribe(const std::string& topic);


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
    std::unordered_map<std::string, Publisher> publishers_;
    std::unordered_map<std::string, Subscriber> subscribers_;
};
} // namespace mini_ipc