#include <mini_ipc/node.hpp>

#include <unistd.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <cstring>

#include <utility>
#include <iostream>

namespace mini_ipc {
class Node::Impl {
public:
    std::string name_;
    int epoll_fd_;

    Impl(std::string name)
        : name_(std::move(name)) {
        epoll_fd_ = epoll_create1(0);
        std::cout << "Node Impl created for: " << name_ << "\n";
    }

    ~Impl() {
        close(epoll_fd_);
        std::cout << "Node Impl destroyed for: " << name_ << "\n";
    }

    // 与discovry_daemon进行一次短连接通信
    std::string talk_to_discovery_daemon(const std::string& msg) {
        int sc_fd;
        if ((sc_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("socket");
            return "";
        }

        struct sockaddr_in server_addr = {};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(8888);
        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

        if (connect(sc_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            perror("connect");
            close(sc_fd);
            std::cout << "[Node] Failed to connect to Discovery Daemon!" << "\n";
            return "";
        }

        if (write(sc_fd, msg.c_str(), msg.length()) == -1) {
            perror("write");
            close(sc_fd);
            return "";
        }

        char buffer[256] = {}; // NOLINT
        if (read(sc_fd, buffer, sizeof(buffer)) == -1) {
            perror("read");
            close(sc_fd);
            return "";
        }

        close(sc_fd);

        return std::string{buffer};
    }

    // 初始化发布者
    bool init_publisher(const std::string& topic_name) {
        int server_fd;
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("socket");
            return false;
        }

        struct sockaddr_in server_addr = {};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = 0;

        if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            perror("bind");
            return false;
        }

        listen(server_fd, 16);

        socklen_t server_addr_len = sizeof(server_addr);
        if (getsockname(server_fd, (struct sockaddr*)&server_addr, &server_addr_len) == -1) {
            perror("getsockname");
            return false;
        }
        unsigned short assigned_port = ntohs(server_addr.sin_port);

        // 向守护进程注册
        std::string reg_msg = "PUB" + topic_name + " " + std::to_string(assigned_port);
        talk_to_discovery_daemon(reg_msg);

        // TODO: 将server_fd加入epoll_fd_
        std::cout << "[Node: " << name_ << "] Publisher for '" << topic_name << "' listening on port " << assigned_port << "\n";

        return true;
    }

    bool init_subscriber(const std::string& topic_name) {
        // 向守护进程查询话题端口信息
        std::string port_info = talk_to_discovery_daemon("SUB" + topic_name);
        if (port_info == "NOT_FOUND" || port_info.empty()) {
            std::cerr << "[Node: " << name_ << "] Topic '" << topic_name << "' not found!" << "\n";
            return false;
        }

        unsigned short target_port = std::stoi(port_info);
        int client_fd;
        if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("socket");
            return false;
        }

        struct sockaddr_in server_addr = {};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(target_port);
        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

        if (connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            perror("connect");
            close(client_fd);
            return false;
        }

        std::cout << "[Node: " << name_ << "] P2P Connected to Publisher on port " << target_port << "\n";

        // TODO: 将 client_fd 加入 epoll_fd_ 监听数据接收
        return true;
    }

    void spin() {
        // TODO: epoll_wait 循环
    }
};

Node::Node(const std::string& node_name)
    : node_name_(node_name), pimpl_(std::make_unique<Impl>(node_name)) {
}

Node::~Node() = default;

Publisher Node::create_publisher(const std::string& topic_name) {
    if (!pimpl_->init_publisher(topic_name)) {
        std::cout << "[Node: " << node_name_ << "]" << "Create Publisher failed" << "\n";
        // TODO: 错误处理
    }
    return Publisher();
}

Subscriber Node::create_subscriber(
    const std::string& topic_name,
    Subscriber::CallbackType callback) {
    if (!pimpl_->init_subscriber(topic_name)) {
        std::cout << "[Node: " << node_name_ << "]" << "Create Subscriber failed" << "\n";
        // TODO: 错误处理
    }

    return Subscriber();
}

// 接口转发
void Node::spin() {
    pimpl_->spin();
}

} // namespace mini_ipc
