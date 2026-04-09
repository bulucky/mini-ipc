#include <mini_ipc/node.hpp>

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include <utility>
#include <iostream>

namespace mini_ipc {
class Node::Impl {
public:
    std::string name_;
    int epoll_fd_;
    int server_socket_;

    Impl(std::string name)
        : name_(std::move(name)) {
        std::cout << "Node Impl created for: " << name_ << "\n";
    }

    ~Impl() {
        std::cout << "Node Impl destroyed for: " << name_ << "\n";
    }

    void spin() {
    }
};

Node::Node(const std::string& node_name)
    : pimpl_(std::make_unique<Impl>(node_name)) {
}

Node::~Node() = default;

// 接口转发
void Node::spin() {
    pimpl_->spin();
}

} // namespace mini_ipc
