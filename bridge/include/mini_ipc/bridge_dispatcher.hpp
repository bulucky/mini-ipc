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
     */
    std::string handle_publish(const std::string& topic,
                               const std::string& payload);

    std::string handle_subscribe(const std::string& topic);

    Publisher& get_or_create_publisher(const std::string& topic);

    static std::string extract_string_field(const std::string& json,
                                            const std::string& key);

    static std::string make_status(const std::string& level,
                                   const std::string& message);

    static std::string make_message(const std::string& topic,
                                    const std::string& payload);
    /**
     *@brief: 转义json
     */
    static std::string escape_json(const std::string& test);

    Node& node_;
    std::unordered_map<std::string, Publisher> publishers_;
    std::unordered_map<std::string, Subscriber> subscribers_;
};
} // namespace mini_ipc