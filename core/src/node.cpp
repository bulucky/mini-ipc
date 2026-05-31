#include <mini_ipc/node.hpp>
#include "mini_ipc/param_manager.hpp"

#include <unistd.h>
#include <sys/uio.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <cstring>

#include <mutex>
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
    std::mutex sub_fds_mutex_;

    Impl(std::string topic_name, int fd)
        : topic_name_(std::move(topic_name)), server_fd_(fd) {}

    ~Impl() {
        for (const auto sub_fd : sub_fds_) {
            close(sub_fd);
        }
        close(server_fd_);
    }

    void publish(const std::string& msg) {
        std::lock_guard<std::mutex> lock(sub_fds_mutex_);

        for (auto it_fd = sub_fds_.begin(); it_fd != sub_fds_.end();) {
            uint32_t net_len = htonl(static_cast<uint32_t>(msg.size()));
            struct iovec iov[2]{
                {&net_len, 4},
                {const_cast<char*>(msg.data()), msg.size()},
            };
            // debug
            // printf("iov_len: %zu", iov->iov_len);

            if (writev(*it_fd, iov, 2) == -1) {
                close(*it_fd);
                it_fd = sub_fds_.erase(it_fd);
            } else {
                ++it_fd;
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

    // 管理等待 discovry 通知的 fd
    std::unordered_map<int, std::string> pending_sub_topics;
    std::unordered_map<int, Subscriber::CallbackType> pending_sub_callbacks;

    // server_fd --> Publisher::Impl
    std::unordered_map<int, std::shared_ptr<Publisher::Impl>> publishers;
    // client_fd --> Subscriber::Callback
    std::unordered_map<int, Subscriber::CallbackType> subscriber_callbacks;

    // subscriber_fd --> buffer
    std::unordered_map<int, std::vector<char>> sub_read_buffers;
    // fd --> topic
    std::unordered_map<int, std::string> sub_topics;

    Impl(std::string name)
        : name_(std::move(name)) {
        epoll_fd_ = epoll_create1(0);
        std::cout << "Node Impl created for: " << name_ << "\n";
    }

    ~Impl() {
        close(epoll_fd_);
        std::cout << "Node Impl destroyed for: " << name_ << "\n";
    }

    /**
     * @brief:  与发现进程(全局唯一，用于维护话题名与端口的映射表)进行一次短连接
     *          用于注册(发布者)或查询(订阅者)话题名与端口的映射关系
     * @param:  const std::string& msg
     * @return: std::string "" --> 注册成功，buffer --> 端口
     */
    std::string talk_to_discovery_daemon(int& sc_fd, const std::string& msg) {
        // int sc_fd;
        if ((sc_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("socket");
            return "";
        }
        auto& params = ParamManager::instance();

        std::string server_ip = params.get<std::string>("discovery_daemon.ip", "127.0.0.1");
        int server_port = params.get<int>("discovery_daemon.port", 8888);

        struct sockaddr_in server_addr = {};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

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
        // close(sc_fd);

        return std::string{buffer};
    }

    /**
     *@brief: 连接publisher
     */
    int connect_to_publisher(unsigned short port,
                             const std::string& topic_name,
                             Subscriber::CallbackType callback) {
        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (client_fd == -1) return -1;

        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (connect(client_fd, (sockaddr*)&addr, sizeof(addr)) == -1) {
            close(client_fd);
            return -1;
        }

        struct epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = client_fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev);

        subscriber_callbacks[client_fd] = std::move(callback);
        sub_topics[client_fd] = topic_name;
        sub_read_buffers[client_fd] = std::vector<char>{};

        return client_fd;
    }

    /**
     * @brief:  初始化发布者资源
     * @param:  const std::string& topic_name 话题名
     * @return: std::shared_ptr<Publisher::Impl> 发布者的impl的地址
     */
    std::shared_ptr<Publisher::Impl>
    init_publisher(const std::string& topic_name) {
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

        auto& params = ParamManager::instance();
        int pub_server_backlog = params.get<int>("publisher.backlog", 16);

        listen(server_fd, pub_server_backlog);

        socklen_t server_addr_len = sizeof(server_addr);
        if (getsockname(server_fd, (struct sockaddr*)&server_addr, &server_addr_len) == -1) {
            perror("getsockname");
            return nullptr;
        }
        unsigned short assigned_port = ntohs(server_addr.sin_port);

        // 向守护进程注册
        // PUB <topi>c <port>
        std::string reg_msg = "PUB " + topic_name + " " + std::to_string(assigned_port);
        int sc_fd = 0;
        talk_to_discovery_daemon(sc_fd, reg_msg);
        close(sc_fd);

        // [TODO]: 将server_fd加入epoll_fd_
        struct epoll_event epoll_ev{};
        epoll_ev.events = EPOLLIN;
        epoll_ev.data.fd = server_fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd, &epoll_ev);

        auto pub_impl = std::make_shared<Publisher::Impl>(topic_name, server_fd);
        publishers[server_fd] = pub_impl;

        std::cout << "[Node: " << name_ << "] Publisher for '" << topic_name << "' listening on port " << assigned_port << "\n";

        return pub_impl;
    }

    /**
     * @brief:  初始化订阅者资源
     * @param:  const std::string& topic_name 话题名
     * @param:  Subscriber::CallbackType callback 回调函数
     * @return: std::shared_ptr<Publisher::Impl> 订阅者的impl的地址
     */
    std::shared_ptr<Subscriber::Impl> init_subscriber(
        const std::string& topic_name, Subscriber::CallbackType callback) {
        // 向守护进程查询话题端口信息
        // SUB <topic>
        int sc_fd = 0;
        std::string response = talk_to_discovery_daemon(sc_fd, "SUB " + topic_name);

        if (response.empty()) {
            std::cerr << "[Node: " << name_ << "] Topic '" << topic_name << "daemon discovery response is empty!" << "\n";
            close(sc_fd);
            return nullptr;
        }

        struct epoll_event epoll_ev{};
        epoll_ev.events = EPOLLIN;
        // 查询话题时发布者已经上线，返回端口
        if (strcmp(response.substr(0, 4).c_str(), "WAIT") != 0) {
            int client_fd = 0;
            unsigned short target_port = std::stoi(response);
            if ((client_fd = connect_to_publisher(target_port, topic_name, callback)) == -1) {
                perror("connect_to_publisher");
                // 连接失败可能publisher已经下线, 退回至等待模式
                pending_sub_topics[sc_fd] = topic_name;
                pending_sub_callbacks[sc_fd] = std::move(callback);

                epoll_ev.data.fd = sc_fd;
                epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, sc_fd, &epoll_ev);

                std::cout
                    << "[Node: " << name_ << "] Publisher maybe offline, Waitting"
                    << "\n ";
                return nullptr;
            }

            close(sc_fd);

            std::cout
                << "[Node: " << name_ << "] P2P Connected to Publisher on port " << target_port
                << "\n";
            return std::make_shared<Subscriber::Impl>(client_fd);
        } else if (strcmp(response.substr(0, 4).c_str(), "WAIT") == 0) {
            // 加入等待列表
            pending_sub_topics[sc_fd] = topic_name;
            pending_sub_callbacks[sc_fd] = std::move(callback);

            epoll_ev.data.fd = sc_fd;
            epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, sc_fd, &epoll_ev);

            std::cout
                << "[Node: " << name_ << "] Waitting Publisher Go online "
                << "\n ";
        }

        return nullptr;
    }

    /**
     * @brief:  节点的impl处理回调
     */
    void spin() {
        auto& params = ParamManager::instance();
        int epoll_max_events = params.get<int>("runtime.epoll_max_events", 1);
        // std::cout << "epoll_max_events" << epoll_max_events << "\n";

        struct epoll_event events[epoll_max_events]; // NOLINT
        std::cout << "[Node: " << name_ << "] Spinning and waiting for events...\n";
        while (true) {
            int event_num = epoll_wait(epoll_fd_, events, 10, -1);
            for (int i = 0; i < event_num; ++i) {
                int active_fd = events[i].data.fd;

                // Publisher的server_fd有新的Subscriber连接
                if (publishers.find(active_fd) != publishers.end()) {
                    int new_sub_fd;
                    if ((new_sub_fd = accept(active_fd, nullptr, nullptr)) != -1) {
                        std::lock_guard<std::mutex> lock(publishers[active_fd]->sub_fds_mutex_);
                        publishers[active_fd]->sub_fds_.push_back(new_sub_fd);
                        std::cout << "[Node: " << name_ << "] Accepted new subscriber connection.\n";
                    }
                }
                // Subscriber 的 client_fd有接受到数据
                else if (subscriber_callbacks.find(active_fd) != subscriber_callbacks.end()) {
                    char buffer[4096] = {0};
                    int read_bytes = read(active_fd, buffer, sizeof(buffer));

                    // EOF, publisher offline
                    if (read_bytes == 0) {
                        auto topic_name = std::move(sub_topics[active_fd]);
                        auto callback = std::move(subscriber_callbacks[active_fd]);

                        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, active_fd, nullptr);
                        close(active_fd);
                        subscriber_callbacks.erase(active_fd);
                        sub_topics.erase(active_fd);
                        sub_read_buffers.erase(active_fd);

                        std::cout << "[Node: " << name_ << "] Publisher disconnected...\n";

                        // publisher offline, subscriber进入pending态
                        int sc_fd = 0;
                        std::string response = talk_to_discovery_daemon(sc_fd, "SUB " + topic_name);
                        if (response.empty() || response.compare(0, 4, "WAIT") == 0) {
                            // publisher 仍未上线, 进入pending态
                            pending_sub_topics[sc_fd] = topic_name;
                            pending_sub_callbacks[sc_fd] = std::move(callback);

                            struct epoll_event ev{};
                            ev.data.fd = sc_fd;
                            ev.events = EPOLLIN;
                            epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, sc_fd, &ev);
                        }
                        // // publisher已经重新上线, 直接连接
                        // else {
                        //     close(sc_fd);
                        //     unsigned short target_port = std::stoi(response);
                        //     int client_fd = 0;
                        //     if ((client_fd = connect_to_publisher(target_port, topic_name, callback)) == -1) {
                        //         perror("connect_to_publisher");

                        //         // int retry_fd = 0;
                        //         // std::string r = talk_to_discovery_daemon(retry_fd, "SUB " + topic_name);
                        //         // pending_sub_topics[retry_fd] = topic_name;
                        //         // pending_sub_callbacks[retry_fd] = std::move(callback);
                        //         // struct epoll_event ev{};
                        //         // ev.events = EPOLLIN;
                        //         // ev.data.fd = retry_fd;
                        //         // epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, retry_fd, &ev);
                        //     }
                        // }

                        break;
                    }
                    // error handle
                    else if (read_bytes == -1) {
                        perror("read");
                        break;
                    }
                    // receive data and parse
                    else {
                        auto& sub_buffer = sub_read_buffers[active_fd];
                        sub_buffer.insert(sub_buffer.end(), buffer, buffer + read_bytes);
                        // parse
                        while (sub_buffer.size() >= 4) {
                            uint32_t net_len;
                            std::memcpy(&net_len, sub_buffer.data(), 4);
                            uint32_t msg_len = ntohl(net_len);

                            // 完整帧 sub_buffer.size() == 4 + msg_len
                            if (sub_buffer.size() < 4 + msg_len) {
                                break;
                            }

                            std::string msg(sub_buffer.data() + 4, msg_len);
                            subscriber_callbacks[active_fd](msg);

                            sub_buffer.erase(sub_buffer.begin(), sub_buffer.begin() + 4 + msg_len);
                        }
                    }
                }
                // pending态对应的publisher online
                else if (pending_sub_topics.find(active_fd) != pending_sub_topics.end()) {
                    char buffer[1024] = {};
                    int read_bytes = read(active_fd, buffer, sizeof(buffer));
                    // daemon discovey offline
                    if (read_bytes <= 0) {
                        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, active_fd, nullptr);
                        close(active_fd);

                        pending_sub_topics.erase(active_fd);
                        pending_sub_callbacks.erase(active_fd);

                        continue;
                    }

                    std::string msg(buffer, read_bytes);
                    if (strcmp(msg.substr(0, 4).c_str(), "PORT") == 0) {
                        // 关闭查询fd, 新建fd用于通信
                        unsigned short target_port = std::stoi(msg.substr(5));
                        auto callback = std::move(pending_sub_callbacks[active_fd]);
                        auto topic_name = std::move(pending_sub_topics[active_fd]);

                        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, active_fd, nullptr);
                        close(active_fd);
                        pending_sub_topics.erase(active_fd);
                        pending_sub_callbacks.erase(active_fd);

                        int client_fd = 0;
                        if ((client_fd = connect_to_publisher(target_port, topic_name, callback)) == -1) {
                            perror("connect_to_publisher");

                            continue;
                        }

                        std::cout << "[Node: " << name_ << "] Publisher go online and connected...\n";
                    }
                }
            }
        }
    }
};

/**
 * @brief:  节点构造函数
 * @param:  const std::string& node_name 话题名
 */
Node::Node(const std::string& node_name)
    : node_name_(node_name), pimpl_(std::make_unique<Impl>(node_name)) {
}

Node::~Node() = default;

/**
 * @brief:  实例化发布者
 * @param:  const std::string& node_name 话题名
 * @return:  Publisher 发布者对象
 */
Publisher Node::create_publisher(const std::string& topic_name) {
    auto pub_impl = pimpl_->init_publisher(topic_name);
    if (!pub_impl) {
        std::cout << "[Node: " << node_name_ << "]" << "Create Publisher failed" << "\n";
        return Publisher{};
    }

    return Publisher{pub_impl};
}

/**
 * @brief:  实例化订阅者者
 * @param:  const std::string& node_name 话题名
 * @param:  Subscriber::CallbackType callback 回调函数
 * @return:  Publisher 订阅者对象
 */
Subscriber Node::create_subscriber(
    const std::string& topic_name, Subscriber::CallbackType callback) {
    auto sub_impl = pimpl_->init_subscriber(topic_name, callback); // NOLINT
    if (!sub_impl) {
        std::cout << "[Node: " << node_name_ << "]" << "Create Subscriber failed" << "\n";
        return Subscriber{};
    }

    return Subscriber{sub_impl};
}

/**
 * @brief:  节点处理回调
 */
void Node::spin() {
    pimpl_->spin();
}

} // namespace mini_ipc
