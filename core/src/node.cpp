#include <mini_ipc/node.hpp>

#include <unistd.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <cstring>

#include <utility>
#include <iostream>
#include <unordered_map>

namespace mini_ipc {

// Publisher代理Impl
class Publisher::Impl {
public:
    std::string topic_name_;
    int server_fd_;
    std::vector<int> sub_fds_;

    Impl(std::string topic_name, int fd)
        : topic_name_(std::move(topic_name)), server_fd_(fd) {}

    ~Impl() {
        for (const auto sub_fd : sub_fds_) {
            close(sub_fd);
        }
        close(server_fd_);
    }

    void publish(const std::string& msg) {
        for (auto sub_fd : sub_fds_) {
            if (write(sub_fd, msg.c_str(), msg.length()) == -1) {
                // [TODO]：错误处理
                continue;
            }
        }
    }
};

// 实现Publisher，暴露给Node对象
Publisher::Publisher(std::shared_ptr<Impl> impl) : pimpl_(std::move(impl)) {}

void Publisher::publish(const std::string& msg) {
    if (pimpl_) {
        pimpl_->publish(msg);
    }
}

// Subscriber代理Impl
class Subscriber::Impl {
public:
    int client_fd_;

    Impl(int client_fd) : client_fd_(client_fd) {};

    ~Impl() {
        close(client_fd_);
    }
};

Subscriber::Subscriber(std::shared_ptr<Impl> impl) : pimpl_(std::move(impl)) {}


class Node::Impl {
public:
    std::string name_;
    int epoll_fd_;

    // server_fd --> Publisher::Impl
    std::unordered_map<int, std::shared_ptr<Publisher::Impl>> publishers_;
    // client_fd --> Subscriber::Callback
    std::unordered_map<int, Subscriber::CallbackType> subscriber_callbacks;


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
    std::shared_ptr<Publisher::Impl> init_publisher(const std::string& topic_name) {
        int server_fd;
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("socket");
            return nullptr;
        }

        struct sockaddr_in server_addr = {};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = 0;

        if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            perror("bind");
            return nullptr;
        }

        listen(server_fd, 16);

        socklen_t server_addr_len = sizeof(server_addr);
        if (getsockname(server_fd, (struct sockaddr*)&server_addr, &server_addr_len) == -1) {
            perror("getsockname");
            return nullptr;
        }
        unsigned short assigned_port = ntohs(server_addr.sin_port);

        // 向守护进程注册
        // PUB <topi>c <port>
        std::string reg_msg = "PUB " + topic_name + " " + std::to_string(assigned_port);
        talk_to_discovery_daemon(reg_msg);

        // [TODO]: 将server_fd加入epoll_fd_
        struct epoll_event epoll_ev{};
        epoll_ev.events = EPOLLIN;
        epoll_ev.data.fd = server_fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd, &epoll_ev);

        auto pub_impl = std::make_shared<Publisher::Impl>(topic_name, server_fd);
        publishers_[server_fd] = pub_impl;

        std::cout << "[Node: " << name_ << "] Publisher for '" << topic_name << "' listening on port " << assigned_port << "\n";

        return pub_impl;
    }

    std::shared_ptr<Subscriber::Impl> init_subscriber(
        const std::string& topic_name, Subscriber::CallbackType callback) {
        // 向守护进程查询话题端口信息
        // SUB <topic>
        std::string port_info = talk_to_discovery_daemon("SUB " + topic_name);
        if (port_info == "NOT_FOUND" || port_info.empty()) {
            std::cerr << "[Node: " << name_ << "] Topic '" << topic_name << "' not found!" << "\n";
            return nullptr;
        }

        unsigned short target_port = std::stoi(port_info);
        int client_fd;
        if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("socket");
            return nullptr;
        }

        struct sockaddr_in server_addr = {};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(target_port);
        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

        if (connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            perror("connect");
            close(client_fd);
            return nullptr;
        }

        // [TODO]: 将 client_fd 加入 epoll_fd_ 监听数据接收
        struct epoll_event epoll_ev{};
        epoll_ev.events = EPOLLIN;
        epoll_ev.data.fd = client_fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &epoll_ev);

        subscriber_callbacks[client_fd] = std::move(callback);

        std::cout
            << "[Node: " << name_ << "] P2P Connected to Publisher on port " << target_port << "\n";

        return std::make_shared<Subscriber::Impl>(client_fd);
    }

    void spin() {
        // [TODO]: epoll_wait 循环
        struct epoll_event events[10]; // NOLINT
        std::cout << "[Node: " << name_ << "] Spinning and waiting for events...\n";
        while (true) {
            int event_num = epoll_wait(epoll_fd_, events, 10, -1);
            for (int i = 0; i < event_num; ++i) {
                int active_fd = events[i].data.fd;

                // Publisher的server_fd有新的Subscriber连接
                if (publishers_.find(active_fd) != publishers_.end()) {
                    int new_sub_fd;
                    if ((new_sub_fd = accept(active_fd, nullptr, nullptr)) != -1) {
                        publishers_[active_fd]->sub_fds_.push_back(new_sub_fd);
                        std::cout << "[Node: " << name_ << "] Accepted new subscriber connection.\n";
                    }
                }
                // Subscriber 的 client_fd有接受到数据
                else if (subscriber_callbacks.find(active_fd) != subscriber_callbacks.end()) {
                    char buffer[1024] = {0};
                    int read_bytes = read(active_fd, buffer, sizeof(buffer));

                    if (read_bytes > 0) {
                        // 触发回调函数
                        std::string msg{buffer};
                        subscriber_callbacks[active_fd](msg);
                    } else if (read_bytes == 0) {
                        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, active_fd, nullptr);
                        close(active_fd);
                        subscriber_callbacks.erase(active_fd);
                        std::cout << "[Node: " << name_ << "] Publisher disconnected.\n";
                    }
                }
            }
        }
    }
};

Node::Node(const std::string& node_name)
    : node_name_(node_name), pimpl_(std::make_unique<Impl>(node_name)) {
}

Node::~Node() = default;

Publisher Node::create_publisher(const std::string& topic_name) {
    auto pub_impl = pimpl_->init_publisher(topic_name);
    if (!pub_impl) {
        std::cout << "[Node: " << node_name_ << "]" << "Create Publisher failed" << "\n";
        return Publisher{};
    }

    return Publisher{pub_impl};
}

Subscriber Node::create_subscriber(
    const std::string& topic_name, Subscriber::CallbackType callback) {
    auto sub_impl = pimpl_->init_subscriber(topic_name, callback); // NOLINT
    if (!sub_impl) {
        std::cout << "[Node: " << node_name_ << "]" << "Create Subscriber failed" << "\n";
        return Subscriber{};
    }

    return Subscriber{sub_impl};
}

// 接口转发
void Node::spin() {
    pimpl_->spin();
}

} // namespace mini_ipc
