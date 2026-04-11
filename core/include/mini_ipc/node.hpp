#pragma once

#include <string>

#include "mini_ipc/publisher.hpp"
#include "mini_ipc/subscriber.hpp"

namespace mini_ipc {

class Node {
public:
    explicit Node(const std::string& node_name);
    ~Node();

    // 禁用拷贝--单例模式
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    // 创建订阅者、发布者
    Publisher create_publisher(const std::string& topic_name);
    Subscriber create_subscriber(const std::string& topic_name,
                                 Subscriber::CallbackType& callback);

    // 启动事件循环、阻塞线程、处理回调
    void spin();

private:
    std::string node_name_;

    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace mini_ipc